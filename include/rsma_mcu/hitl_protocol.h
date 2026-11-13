#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rsma_mcu {

enum class HitlMessageType : std::uint8_t
{
    Hello = 1,
    HelloAck = 2,
    Heartbeat = 3,
    HeartbeatAck = 4,
    Data = 5,
    SafeState = 6,
};

struct HitlFrame
{
    std::uint8_t version = 1;
    HitlMessageType type = HitlMessageType::Data;
    std::uint16_t flags = 0;
    std::uint32_t sequence = 0;
    std::uint64_t timestampMilliseconds = 0;
    std::vector<std::uint8_t> payload;
};

constexpr std::uint8_t kHitlProtocolVersion = 1;
constexpr std::size_t kHitlHeaderSize = 22;
constexpr std::size_t kHitlChecksumSize = 4;
constexpr std::size_t kHitlMaxPayloadSize = 0xFFFF;

std::uint32_t hitl_crc32(std::span<const std::uint8_t> bytes);
std::vector<std::uint8_t> encode_hitl_frame(const HitlFrame& frame);
bool decode_hitl_frame(std::span<const std::uint8_t> bytes, HitlFrame& frame, std::string& error);

} // namespace rsma_mcu