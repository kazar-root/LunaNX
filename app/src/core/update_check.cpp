#include "update_check.hpp"

#include "app_version.hpp"
#include "http_client.hpp"
#include "runtime_journal.hpp"

#include <jansson.h>

#include <exception>
#include <memory>

namespace lunanx
{
namespace
{

constexpr const char* kLatestReleaseUrl = "https://api.github.com/repos/kazar-root/LunaNX/releases/latest";

} // namespace

UpdateCheckResult CheckForUpdate()
{
    UpdateCheckResult result;
    try
    {
        HttpClient http;
        const HttpResponse response = http.Get(
            kLatestReleaseUrl, std::string("LunaNX/") + kAppVersion,
            {"Accept: application/vnd.github+json", "X-GitHub-Api-Version: 2022-11-28"}, 10, 20);
        if (response.status_code != 200)
        {
            LogRuntimeEvent("update", "check_failed", "http=" + std::to_string(response.status_code));
            return result;
        }

        json_error_t error {};
        std::unique_ptr<json_t, decltype(&json_decref)> root(
            json_loads(response.body.c_str(), 0, &error), &json_decref);
        const char* tag = json_string_value(json_object_get(root.get(), "tag_name"));
        if (!tag || ParseVersionNumbers(tag).empty())
        {
            LogRuntimeEvent("update", "check_failed", "invalid release");
            return result;
        }

        result.latest_version = tag;
        if (result.latest_version[0] == 'v' || result.latest_version[0] == 'V')
            result.latest_version.erase(0, 1);
        result.update_available = CompareVersions(result.latest_version, kAppVersion) > 0;
        result.ok = true;
        LogRuntimeEvent("update", "checked", "latest=" + result.latest_version);
    }
    catch (const std::exception& ex)
    {
        LogRuntimeEvent("update", "check_failed", ex.what());
    }
    return result;
}

} // namespace lunanx
