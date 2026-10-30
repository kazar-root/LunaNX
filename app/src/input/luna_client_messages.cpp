#include "luna_client_messages.hpp"

#include "luna_proto.hpp"

#include <string>

namespace lunanx::input
{
namespace
{

constexpr std::uint32_t kPingField = 20;
constexpr std::uint32_t kHelloField = 30;
constexpr std::uint32_t kDisplayField = 40;
constexpr std::uint32_t kClientInfoField = 42;
constexpr std::uint32_t kRumbleField = 60;
constexpr std::size_t kServerHeaderSize = 16;

// Fonctionnalités annoncées par le client web à l'ouverture et dans la demande d'affichage.
const char* const kHelloStrings[] = {"server-reconfigure", "client-display-config-flag"};
const char* const kDisplayProfile = "dynamic-downsampling";

bool ReadVarint(const std::uint8_t* data, std::size_t size, std::size_t& pos, std::uint64_t& value)
{
    value = 0;
    for (int shift = 0; pos < size && shift < 64; shift += 7)
    {
        const std::uint8_t byte = data[pos++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            return true;
    }
    return false;
}

} // namespace

std::vector<std::uint8_t> EncodeSessionHello()
{
    ProtoWriter hello;
    for (const char* value : kHelloStrings)
        if (*value)
            hello.String(1, value);
    ProtoWriter message;
    message.Message(kHelloField, hello);
    return message.data();
}

std::vector<std::uint8_t> EncodeDisplayRequest(const DisplayRequest& request)
{
    ProtoWriter resolution;
    resolution.Varint(1, static_cast<std::uint64_t>(request.width))
        .Varint(2, static_cast<std::uint64_t>(request.height));
    ProtoWriter mode;
    mode.Message(1, resolution).Varint(2, static_cast<std::uint64_t>(request.fps)).Message(3, ProtoWriter());
    ProtoWriter unknown;
    unknown.Varint(2, 100).Varint(3, 52).Varint(5, 4);

    ProtoWriter display;
    display.Varint(1, 1).Message(3, mode).Message(4, unknown);
    if (*kDisplayProfile)
        display.String(5, kDisplayProfile);
    display.Varint(6, request.min_bitrate)
        .Varint(7, request.max_bitrate)
        .Varint(8, request.start_bitrate)
        .Varint(9, 1);

    ProtoWriter message;
    message.Message(kDisplayField, display);
    return message.data();
}

std::vector<std::uint8_t> EncodeClientInfo()
{
    // Mêmes valeurs que le clientContext envoyé lors de la création de session.
    ProtoWriter info;
    info.String(1, "Chrome")
        .String(2, "141.0.0.0")
        .String(3, "Windows")
        .String(4, "Windows")
        .String(5, "Windows")
        .String(6, "10");
    ProtoWriter client;
    client.Message(1, info);
    ProtoWriter message;
    message.Message(kClientInfoField, client);
    return message.data();
}

std::vector<std::uint8_t> EncodePing(std::uint64_t now_ms)
{
    ProtoWriter ping;
    ping.Varint(1, now_ms);
    ProtoWriter message;
    message.Message(kPingField, ping);
    return message.data();
}

std::uint32_t ServerMessageField(const std::uint8_t* data, std::size_t size)
{
    if (size <= kServerHeaderSize)
        return 0;
    std::uint32_t key = 0;
    int shift = 0;
    for (std::size_t i = kServerHeaderSize; i < size && shift < 32; ++i, shift += 7)
    {
        key |= static_cast<std::uint32_t>(data[i] & 0x7f) << shift;
        if (!(data[i] & 0x80))
            return key >> 3;
    }
    return 0;
}

bool ParseRumble(const std::uint8_t* data, std::size_t size, RumbleCommand& out)
{
    if (size <= kServerHeaderSize)
        return false;
    std::size_t pos = kServerHeaderSize;
    std::uint64_t key = 0;
    std::uint64_t length = 0;
    if (!ReadVarint(data, size, pos, key) || key != ((kRumbleField << 3) | 2) ||
        !ReadVarint(data, size, pos, length) || length > size - pos)
        return false;

    out = RumbleCommand {};
    const std::size_t end = pos + static_cast<std::size_t>(length);
    while (pos < end)
    {
        std::uint64_t field = 0;
        std::uint64_t value = 0;
        if (!ReadVarint(data, end, pos, field) || (field & 7) != 0 || !ReadVarint(data, end, pos, value))
            break;
        const auto level = static_cast<std::uint8_t>(value > 255 ? 255 : value);
        if ((field >> 3) == 3)
            out.heavy = level;
        else if ((field >> 3) == 4)
            out.light = level;
    }
    return true;
}

} // namespace lunanx::input
