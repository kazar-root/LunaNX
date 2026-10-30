#include "luna_input_encoder.hpp"

#include "luna_proto.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace lunanx::input
{
namespace
{

constexpr std::uint32_t kGamepadStateField = 104;
constexpr std::uint32_t kInputReadyField = 36;

// XInput [-32768, 32767] → échelle Luna [0, 65534], 32767 au repos.
std::uint64_t AxisToLuna(int value)
{
    return static_cast<std::uint64_t>(std::clamp(value + 32767, 0, 65534));
}

std::uint64_t TriggerToLuna(std::uint8_t value)
{
    return static_cast<std::uint64_t>(value) * 65534u / 255u;
}

} // namespace

std::int16_t AxisToXInput(float value)
{
    const float clamped = std::clamp(value, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
}

std::uint64_t WallClockMilliseconds()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::vector<std::uint8_t> LunaInputEncoder::EncodeGamepad(const GamepadState& state)
{
    const std::uint64_t now = WallClockMilliseconds();
    ProtoWriter timing;
    timing.Varint(1, now).Varint(3, 4).Varint(8, now);

    ProtoWriter pad;
    pad.Varint(2, ++sequence_)
        .Varint(3, state.buttons)
        .Varint(4, TriggerToLuna(state.left_trigger))
        .Varint(5, TriggerToLuna(state.right_trigger))
        .Varint(6, AxisToLuna(state.left_x))
        .Varint(7, AxisToLuna(state.left_y))
        .Varint(8, AxisToLuna(state.right_x))
        .Varint(9, AxisToLuna(state.right_y))
        .Message(12, timing);

    ProtoWriter message;
    message.Message(kGamepadStateField, pad);
    return message.data();
}

std::vector<std::uint8_t> LunaInputEncoder::EncodeInputReady()
{
    ProtoWriter message;
    message.Message(kInputReadyField, ProtoWriter());
    return message.data();
}

const std::string& LunaInputEncoder::InputChannelLabel()
{
    static const std::string label = "inputChannel";
    return label;
}

const std::string& LunaInputEncoder::ClientMessageChannelLabel()
{
    static const std::string label = "clientMessageChannel";
    return label;
}

const std::vector<std::string>& LunaInputEncoder::RequestedChannels()
{
    // « display » est confirmé par la capture ; les trois autres noms restent à confirmer.
    static const std::vector<std::string> channels = {"display", "input", "audio", "control"};
    return channels;
}

} // namespace lunanx::input
