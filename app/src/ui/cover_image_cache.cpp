#include "cover_image_cache.hpp"

#include "core/http_client.hpp"

#include <nanovg/stb_image.h>


#ifdef __SWITCH__
#include <sys/stat.h>
#endif

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lunanx
{
namespace
{

constexpr const char* kFallbackCoverRes = "img/lunanx_cover.png";
constexpr size_t kMemoryCacheLimit = 16 * 1024 * 1024;
constexpr size_t kMaximumCachedImageSize = 6 * 1024 * 1024;
constexpr int kImageWorkerCount = 3;
constexpr int kPosterWidth = 300;
constexpr int kGalleryWidth = 1120;

enum class ImagePriority : int
{
    Prefetch = 0,
    Normal = 1,
    Critical = 2,
};

using ImageData = std::shared_ptr<const std::string>;
using Completion = std::function<void(ImageData)>;

void EnsureImageCacheDirectory()
{
#ifdef __SWITCH__
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/LunaNX", 0777);
    mkdir("sdmc:/switch/LunaNX/cache", 0777);
    mkdir("sdmc:/switch/LunaNX/cache/images", 0777);
#endif
}

std::string HashUrl(const std::string& url)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : url)
    {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }

    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

std::string CachePathForUrl(const std::string& url)
{
    return "sdmc:/switch/LunaNX/cache/images/" + HashUrl(url) + ".img";
}

bool HasImageSignature(const std::string& data)
{
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xff &&
        static_cast<unsigned char>(data[1]) == 0xd8 &&
        static_cast<unsigned char>(data[2]) == 0xff)
    {
        return true;
    }

    static constexpr unsigned char kPngSignature[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    };
    if (data.size() >= sizeof(kPngSignature) &&
        std::equal(
            std::begin(kPngSignature), std::end(kPngSignature),
            reinterpret_cast<const unsigned char*>(data.data())))
    {
        return true;
    }

    if (data.size() >= 6 &&
        (data.compare(0, 6, "GIF87a") == 0 ||
         data.compare(0, 6, "GIF89a") == 0))
    {
        return true;
    }

    // BMP : jaquettes réduites par ShrinkImage.
    if (data.size() >= 54 && data.compare(0, 2, "BM") == 0)
        return true;

    return data.size() >= 12 &&
           data.compare(0, 4, "RIFF") == 0 &&
           data.compare(8, 4, "WEBP") == 0;
}

// Image beaucoup plus grande que son affichage (adresse qu'Amazon ne redimensionne pas) :
// réduite ici à `max_width` pixels de large, en BMP 24 bits. Sans cela une seule jaquette 4K
// occuperait ~33 Mo de texture, et quelques-unes suffisent à épuiser la mémoire de la Switch.
std::string ShrinkImage(const std::string& bytes, int max_width)
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    const int size = static_cast<int>(bytes.size());
    int width = 0;
    int height = 0;
    int channels = 0;
    if (max_width <= 0 || !stbi_info_from_memory(data, size, &width, &height, &channels) ||
        (width <= max_width * 2 && height <= max_width * 3))
        return bytes;

    // Une seule réduction à la fois : l'image décodée peut peser des dizaines de Mo.
    static std::mutex shrink_mutex;
    std::lock_guard<std::mutex> lock(shrink_mutex);
    unsigned char* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 3);
    if (!pixels)
        return {};
    const int target_width = std::min(max_width, width);
    const int target_height = std::max(1, static_cast<int>(static_cast<std::int64_t>(height) * target_width / width));
    const int row_bytes = (target_width * 3 + 3) & ~3;
    const std::uint32_t pixel_bytes = static_cast<std::uint32_t>(row_bytes) * target_height;

    std::string bmp(54 + pixel_bytes, '\0');
    auto put32 = [&bmp](size_t offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            bmp[offset + i] = static_cast<char>((value >> (8 * i)) & 0xff);
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(2, static_cast<std::uint32_t>(bmp.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, static_cast<std::uint32_t>(target_width));
    put32(22, static_cast<std::uint32_t>(target_height));
    bmp[26] = 1;
    bmp[28] = 24;
    put32(34, pixel_bytes);

    // Moyenne de chaque bloc source ; lignes de bas en haut, pixels en BGR (format BMP).
    for (int y = 0; y < target_height; ++y)
    {
        const int y0 = static_cast<int>(static_cast<std::int64_t>(y) * height / target_height);
        const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<std::int64_t>(y + 1) * height / target_height));
        char* out = &bmp[54 + static_cast<size_t>(target_height - 1 - y) * row_bytes];
        for (int x = 0; x < target_width; ++x)
        {
            const int x0 = static_cast<int>(static_cast<std::int64_t>(x) * width / target_width);
            const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<std::int64_t>(x + 1) * width / target_width));
            std::uint32_t sum[3] = {0, 0, 0};
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = x0; sx < x1; ++sx)
                {
                    const unsigned char* p = pixels + (static_cast<size_t>(sy) * width + sx) * 3;
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                }
            const std::uint32_t count = static_cast<std::uint32_t>((y1 - y0) * (x1 - x0));
            out[x * 3 + 0] = static_cast<char>(sum[2] / count);
            out[x * 3 + 1] = static_cast<char>(sum[1] / count);
            out[x * 3 + 2] = static_cast<char>(sum[0] / count);
        }
    }
    stbi_image_free(pixels);
    return bmp;
}

bool ReadCachedImage(const std::string& path, std::string& data)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream.is_open())
        return false;

    const std::streampos size = stream.tellg();
    if (size <= 0 ||
        static_cast<std::uint64_t>(size) > kMaximumCachedImageSize)
    {
        stream.close();
        std::remove(path.c_str());
        return false;
    }

    stream.seekg(0, std::ios::beg);
    data.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    if (!data.empty() && HasImageSignature(data))
        return true;

    stream.close();
    std::remove(path.c_str());
    data.clear();
    return false;
}

void WriteCachedImage(const std::string& path, const std::string& data)
{
    if (data.empty() || !HasImageSignature(data))
        return;

    EnsureImageCacheDirectory();
    const std::string temporary_path = path + ".tmp";
    {
        std::ofstream stream(
            temporary_path, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
            return;

        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
        stream.flush();
        if (!stream.good())
        {
            stream.close();
            std::remove(temporary_path.c_str());
            return;
        }
    }

    std::remove(path.c_str());
    if (std::rename(temporary_path.c_str(), path.c_str()) != 0)
        std::remove(temporary_path.c_str());
}

class ImageLoader
{
  public:
    ImageLoader()
    {
        workers_.reserve(kImageWorkerCount);
        for (int index = 0; index < kImageWorkerCount; ++index)
            workers_.emplace_back([this]() { WorkerLoop(); });
    }

    ~ImageLoader()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            critical_queue_.clear();
            normal_queue_.clear();
            prefetch_queue_.clear();
        }
        condition_.notify_all();
        for (auto& worker : workers_)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    // `max_width` : largeur au-delà de laquelle l'image est réduite (voir ShrinkImage).
    void Request(
        const std::string& image_url,
        int max_width,
        ImagePriority priority,
        Completion completion = {})
    {
        // L'image réduite est mise en cache sous sa propre clé.
        const std::string url = image_url + "#w" + std::to_string(max_width);
        ImageData memory_hit;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto cached = memory_cache_.find(url);
            if (cached != memory_cache_.end())
            {
                cached->second.last_used = ++access_counter_;
                memory_hit = cached->second.data;
            }
            else
            {
                auto pending = pending_.find(url);
                if (pending != pending_.end())
                {
                    if (completion)
                        pending->second->completions.push_back(
                            std::move(completion));
                    if (!pending->second->started &&
                        static_cast<int>(priority) >
                            static_cast<int>(pending->second->priority))
                    {
                        pending->second->priority = priority;
                        Queue(url, priority);
                        condition_.notify_one();
                    }
                    return;
                }

                auto state = std::make_shared<PendingRequest>();
                state->priority = priority;
                state->url = image_url;
                state->max_width = max_width;
                if (completion)
                    state->completions.push_back(std::move(completion));
                pending_.emplace(url, std::move(state));
                Queue(url, priority);
                condition_.notify_one();
                return;
            }
        }

        if (completion)
            completion(std::move(memory_hit));
    }

  private:
    struct MemoryEntry
    {
        ImageData data;
        std::uint64_t last_used = 0;
    };

    struct PendingRequest
    {
        ImagePriority priority = ImagePriority::Normal;
        std::string url;
        int max_width = 0;
        bool started = false;
        std::vector<Completion> completions;
    };

    struct QueueEntry
    {
        std::string url;
        ImagePriority priority = ImagePriority::Normal;
    };

    void Queue(const std::string& url, ImagePriority priority)
    {
        QueueEntry entry {url, priority};
        if (priority == ImagePriority::Critical)
            critical_queue_.push_back(std::move(entry));
        else if (priority == ImagePriority::Normal)
            normal_queue_.push_back(std::move(entry));
        else
            prefetch_queue_.push_back(std::move(entry));
    }

    bool PopNext(QueueEntry& entry)
    {
        auto pop = [&entry](std::deque<QueueEntry>& queue) {
            if (queue.empty())
                return false;
            entry = std::move(queue.front());
            queue.pop_front();
            return true;
        };
        return pop(critical_queue_) ||
               pop(normal_queue_) ||
               pop(prefetch_queue_);
    }

    ImageData Load(const std::string& key, const std::string& url, int max_width)
    {
        try
        {
            const std::string cache_path = CachePathForUrl(key);
            std::string bytes;
            if (ReadCachedImage(cache_path, bytes))
                return std::make_shared<const std::string>(std::move(bytes));

            HttpClient http_client;
            const HttpResponse response = http_client.Get(
                url,
                kBrowserUserAgent,
                {"Accept: image/jpeg,image/png,image/*,*/*;q=0.8"},
                5,
                15);
            if (response.status_code != 200 ||
                response.body.empty() ||
                response.body.size() > kMaximumCachedImageSize ||
                !HasImageSignature(response.body))
            {
                return {};
            }

            std::string image = ShrinkImage(response.body, max_width);
            if (image.empty())
                return {};
            WriteCachedImage(cache_path, image);
            return std::make_shared<const std::string>(std::move(image));
        }
        catch (...)
        {
            return {};
        }
    }

    void InsertMemoryCache(const std::string& url, const ImageData& data)
    {
        if (!data || data->empty() || data->size() > kMaximumCachedImageSize)
            return;

        const auto existing = memory_cache_.find(url);
        if (existing != memory_cache_.end())
            memory_cache_bytes_ -= existing->second.data->size();

        MemoryEntry entry;
        entry.data = data;
        entry.last_used = ++access_counter_;
        memory_cache_bytes_ += data->size();
        memory_cache_[url] = std::move(entry);

        while (memory_cache_bytes_ > kMemoryCacheLimit &&
               memory_cache_.size() > 1)
        {
            auto oldest = std::min_element(
                memory_cache_.begin(), memory_cache_.end(),
                [](const auto& left, const auto& right) {
                    return left.second.last_used < right.second.last_used;
                });
            if (oldest == memory_cache_.end())
                break;
            memory_cache_bytes_ -= oldest->second.data->size();
            memory_cache_.erase(oldest);
        }
    }

    void WorkerLoop()
    {
        while (true)
        {
            QueueEntry entry;
            std::shared_ptr<PendingRequest> state;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() {
                    return stopping_ ||
                           !critical_queue_.empty() ||
                           !normal_queue_.empty() ||
                           !prefetch_queue_.empty();
                });
                if (stopping_)
                    return;
                if (!PopNext(entry))
                    continue;

                const auto pending = pending_.find(entry.url);
                if (pending == pending_.end() ||
                    pending->second->started ||
                    pending->second->priority != entry.priority)
                {
                    continue;
                }
                state = pending->second;
                state->started = true;
            }

            ImageData data = Load(entry.url, state->url, state->max_width);
            std::vector<Completion> completions;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopping_)
                    return;
                if (data)
                    InsertMemoryCache(entry.url, data);
                completions = std::move(state->completions);
                pending_.erase(entry.url);
            }

            for (auto& completion : completions)
            {
                if (completion)
                {
                    try
                    {
                        completion(data);
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<QueueEntry> critical_queue_;
    std::deque<QueueEntry> normal_queue_;
    std::deque<QueueEntry> prefetch_queue_;
    std::unordered_map<std::string, std::shared_ptr<PendingRequest>> pending_;
    std::unordered_map<std::string, MemoryEntry> memory_cache_;
    std::vector<std::thread> workers_;
    size_t memory_cache_bytes_ = 0;
    std::uint64_t access_counter_ = 0;
    bool stopping_ = false;
};

std::mutex g_loader_mutex;
ImageLoader* g_loader = nullptr;

ImageLoader& Loader()
{
    std::lock_guard<std::mutex> lock(g_loader_mutex);
    if (!g_loader)
        g_loader = new ImageLoader();
    return *g_loader;
}

void SetCachedRemoteImage(
    brls::Image* image,
    const std::string& image_url,
    int max_width,
    ImagePriority priority,
    std::function<bool()> should_apply = {})
{
    if (!image || image_url.empty())
        return;

    // Keep freeTexture=true. Replacing a resource-backed texture with an
    // async memory texture otherwise leaks GPU textures in Borealis.
    image->setImageAsync(
        [image_url, max_width, priority, should_apply = std::move(should_apply)](
            auto ready) mutable {
            Loader().Request(
                image_url,
                max_width,
                priority,
                [ready, should_apply = std::move(should_apply)](
                    ImageData data) mutable {
                    if (!data || (should_apply && !should_apply()))
                    {
                        ready({}, 0);
                        return;
                    }
                    ready(*data, data->size());
                });
        });
}

} // namespace

void SetCachedPosterImage(brls::Image* image, const std::string& image_url)
{
    if (!image)
        return;
    if (image_url.empty())
    {
        image->setImageFromRes(kFallbackCoverRes);
        return;
    }
    SetCachedRemoteImage(image, ResizeAmazonImageUrl(image_url, kPosterWidth), kPosterWidth, ImagePriority::Normal);
}

void SetCachedGalleryImage(
    brls::Image* image,
    const std::string& image_url,
    std::function<bool()> should_apply)
{
    SetCachedRemoteImage(
        image,
        ResizeAmazonImageUrl(image_url, kGalleryWidth),
        kGalleryWidth,
        ImagePriority::Critical,
        std::move(should_apply));
}

void ShutdownImageCache()
{
    ImageLoader* loader = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_loader_mutex);
        loader = std::exchange(g_loader, nullptr);
    }
    delete loader;
}

} // namespace lunanx
