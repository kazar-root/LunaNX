// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=82155d7451c7eec04c36
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <string_view>

namespace lunanx::luna
{

enum class DeviceCodePollResult { Success, Pending, Retry, Rejected };

inline DeviceCodePollResult ClassifyDeviceCodePoll(long status, std::string_view error, bool has_success)
{
    if (status == 200 && has_success)
        return DeviceCodePollResult::Success;
    if (status == 401 && (error == "Unauthorized" || error == "unauthorized"))
        return DeviceCodePollResult::Pending;
    if (status == 429 || (status >= 500 && status <= 599))
        return DeviceCodePollResult::Retry;
    return DeviceCodePollResult::Rejected;
}

} // namespace lunanx::luna
