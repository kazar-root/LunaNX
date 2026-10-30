#pragma once
#include "IFFmpegVideoDecoder.hpp"
#include "AVFrameHolder.hpp"

class FFmpegVideoDecoder : public IFFmpegVideoDecoder {
  public:
    FFmpegVideoDecoder();
    ~FFmpegVideoDecoder();

    int setup(int video_format, int width, int height, int redraw_rate,
              void* context, int dr_flags) override;
    void cleanup() override;
    int submit_decode_unit(uint8_t* indata, int inlen, int64_t pts = AV_NOPTS_VALUE) override;
    void reset_stream();
    int capabilities() const override;
    VideoDecodeStats* video_decode_stats() override;
    bool uses_hardware_frames() const override { return m_uses_hardware_frames; }
    const std::string& setup_diagnostics() const override { return m_setup_diagnostics; }
    uint64_t corrupt_frames_dropped() const override {
        return static_cast<uint64_t>(m_corrupt_frames_dropped);
    }

  private:
    static AVPixelFormat select_hardware_format(AVCodecContext* context,
                                                const AVPixelFormat* formats);
    int fail_setup(const char* stage, int error_code, const std::string& detail = {});
    void append_setup_diagnostic(const std::string& value);
    int decode(char* indata, int inlen, int64_t pts);
    AVFrame* get_frame(bool native_frame);

    AVPacket* m_packet = nullptr;
    AVBufferRef *hw_device_ctx = nullptr;
    const AVCodec* m_decoder = nullptr;
    AVCodecContext* m_decoder_context = nullptr;
    AVFrame *tmp_frame = nullptr;
    AVFrame** m_frames = nullptr;
    int m_frames_size = 0;

    int m_stream_fps = 0;
    int m_frames_in = 0;
    int m_frames_out = 0;
    int m_corrupt_frames_dropped = 0;
    int m_current_frame = 0, m_next_frame = 0;
    uint32_t m_last_frame = 0;

    VideoDecodeStats m_video_decode_stats_progress = {};
    VideoDecodeStats m_video_decode_stats_cache = {};
    uint64_t timeCount = 0;

    char* m_ffmpeg_buffer = nullptr;
    AVFrame* m_frame = nullptr;
    bool m_uses_hardware_frames = false;
    bool m_requires_hardware_frames = false;
    bool m_first_output_format_logged = false;
    std::string m_setup_diagnostics;
};
