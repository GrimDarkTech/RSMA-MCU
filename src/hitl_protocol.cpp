#include "rsma_mcu/hitl_protocol.h"

#include <algorithm>
#include <array>

namespace rsma_mcu {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'R', 'S', 'M', 'H'};

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
}

void write_u64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8);
    return value;
}

std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
    return value;
}

bool is_valid_message_type(std::uint8_t type)
{
    return type >= static_cast<std::uint8_t>(HitlMessageType::Hello) &&
        type <= static_cast<std::uint8_t>(HitlMessageType::SafeState);
}

} // namespace

std::uint32_t hitl_crc32(std::span<const std::uint8_t> bytes)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const auto value : bytes) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) != 0 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

std::vector<std::uint8_t> encode_hitl_frame(const HitlFrame& frame)
{
    if (frame.version != kHitlProtocolVersion)
        return {};
    const auto type = static_cast<std::uint8_t>(frame.type);
    if (!is_valid_message_type(type) || frame.payload.size() > kHitlMaxPayloadSize)
        return {};

    std::vector<std::uint8_t> bytes(kMagic.begin(), kMagic.end());
    bytes.resize(kHitlHeaderSize + frame.payload.size() + kHitlChecksumSize);
    bytes[4] = frame.version;
    bytes[5] = type;
    write_u16(bytes, 6, frame.flags);
    write_u32(bytes, 8, frame.sequence);
    write_u64(bytes, 12, frame.timestampMilliseconds);
    write_u16(bytes, 20, static_cast<std::uint16_t>(frame.payload.size()));
    std::copy(frame.payload.begin(), frame.payload.end(), bytes.begin() + kHitlHeaderSize);

    const auto checksum = hitl_crc32(std::span<const std::uint8_t>(bytes).subspan(4, bytes.size() - 8));
    write_u32(bytes, bytes.size() - kHitlChecksumSize, checksum);
    return bytes;
}

bool decode_hitl_frame(std::span<const std::uint8_t> bytes, HitlFrame& frame, std::string& error)
{
    if (bytes.size() < kHitlHeaderSize + kHitlChecksumSize) {
        error = "HITL frame is truncated";
        return false;
    }
    for (std::size_t index = 0; index < kMagic.size(); ++index) {
        if (bytes[index] != kMagic[index]) {
            error = "HITL frame magic mismatch";
            return false;
        }
    }
    if (bytes[4] != kHitlProtocolVersion) {
        error = "Unsupported HITL protocol version";
        return false;
    }
    if (!is_valid_message_type(bytes[5])) {
        error = "Unknown HITL message type";
        return false;
    }

    const auto payloadSize = read_u16(bytes, 20);
    if (bytes.size() != kHitlHeaderSize + payloadSize + kHitlChecksumSize) {
        error = "HITL frame length mismatch";
        return false;
    }
    const auto expectedChecksum = read_u32(bytes, bytes.size() - kHitlChecksumSize);
    const auto actualChecksum = hitl_crc32(bytes.subspan(4, bytes.size() - 8));
    if (expectedChecksum != actualChecksum) {
        error = "HITL frame CRC mismatch";
        return false;
    }

    frame.version = bytes[4];
    frame.type = static_cast<HitlMessageType>(bytes[5]);
    frame.flags = read_u16(bytes, 6);
    frame.sequence = read_u32(bytes, 8);
    frame.timestampMilliseconds = read_u64(bytes, 12);
    frame.payload.assign(bytes.begin() + kHitlHeaderSize, bytes.end() - kHitlChecksumSize);
    error.clear();
    return true;
}

} // namespace rsma_mcu