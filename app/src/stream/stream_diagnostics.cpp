// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=4b831aed3b086b117f0f
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u4b831aed3b086b117f0f, "4b831aed3b086b117f0f")

#include "stream_diagnostics.hpp"

#include <atomic>

namespace lunanx
{
namespace
{

std::atomic_bool g_stream_diagnostics_enabled {false};
std::atomic_bool g_sensitive_input_logging_suppressed {false};

} // namespace

void SetStreamDiagnosticsEnabled(bool enabled)
{
    g_stream_diagnostics_enabled.store(enabled, std::memory_order_release);
}

bool StreamDiagnosticsEnabled()
{
    return g_stream_diagnostics_enabled.load(std::memory_order_acquire);
}

void SetSensitiveInputLoggingSuppressed(bool suppressed)
{
    g_sensitive_input_logging_suppressed.store(suppressed, std::memory_order_release);
}

bool SensitiveInputLoggingSuppressed()
{
    return g_sensitive_input_logging_suppressed.load(std::memory_order_acquire);
}

} // namespace lunanx
