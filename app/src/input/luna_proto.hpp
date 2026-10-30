#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lunanx::input
{

// Écriture protobuf minimale : les data channels de Luna échangent des messages protobuf
// (varints et sous-messages uniquement). Les valeurs nulles sont omises, comme en proto3.
class ProtoWriter
{
  public:
    ProtoWriter& Varint(std::uint32_t field, std::uint64_t value)
    {
        if (value == 0)
            return *this;
        Key(field, 0);
        PutVarint(value);
        return *this;
    }

    ProtoWriter& Bytes(std::uint32_t field, const std::uint8_t* data, std::size_t size)
    {
        Key(field, 2);
        PutVarint(size);
        out_.insert(out_.end(), data, data + size);
        return *this;
    }

    ProtoWriter& String(std::uint32_t field, const std::string& value)
    {
        return Bytes(field, reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }

    // Sous-message, écrit même vide (le navigateur envoie certains messages vides).
    ProtoWriter& Message(std::uint32_t field, const ProtoWriter& message)
    {
        return Bytes(field, message.out_.data(), message.out_.size());
    }

    const std::vector<std::uint8_t>& data() const { return out_; }

  private:
    void Key(std::uint32_t field, std::uint32_t wire_type) { PutVarint((field << 3) | wire_type); }

    void PutVarint(std::uint64_t value)
    {
        while (value >= 0x80)
        {
            out_.push_back(static_cast<std::uint8_t>(value | 0x80));
            value >>= 7;
        }
        out_.push_back(static_cast<std::uint8_t>(value));
    }

    std::vector<std::uint8_t> out_;
};

} // namespace lunanx::input
