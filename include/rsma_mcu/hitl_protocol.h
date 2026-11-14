#pragma once

#include <cstdint>
#include <deque>
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
constexpr std::size_t kHitlMaxFrameSize = kHitlHeaderSize + kHitlMaxPayloadSize + kHitlChecksumSize;

std::uint32_t hitl_crc32(std::span<const std::uint8_t> bytes);
std::vector<std::uint8_t> encode_hitl_frame(const HitlFrame& frame);
bool decode_hitl_frame(std::span<const std::uint8_t> bytes, HitlFrame& frame, std::string& error);

class HitlFrameStreamDecoder
{
public:
    std::vector<HitlFrame> feed(std::span<const std::uint8_t> bytes);
    void reset();

    const std::string& lastError() const { return lastError_; }
    std::size_t rejectedFrameCount() const { return rejectedFrameCount_; }

private:
    std::vector<std::uint8_t> buffer_;
    std::string lastError_;
    std::size_t rejectedFrameCount_ = 0;
};

// Host/backend transport boundary. It owns framing state and exposes only
// validated frames to consumers; transport chunks may be fragmented or
// concatenated and may be arbitrarily larger than one frame.
class HitlTransportIngress
{
public:
    static constexpr std::size_t kMaxPendingFrames = 256;

    void ingest(std::span<const std::uint8_t> bytes);
    bool try_pop(HitlFrame& frame);
    void reset();

    std::size_t pendingFrameCount() const { return pendingFrames_.size(); }
    std::size_t rejectedFrameCount() const { return decoder_.rejectedFrameCount(); }
    std::size_t droppedFrameCount() const { return droppedFrameCount_; }
    const std::string& lastError() const { return decoder_.lastError(); }

private:
    HitlFrameStreamDecoder decoder_;
    std::deque<HitlFrame> pendingFrames_;
    std::size_t droppedFrameCount_ = 0;
};

} // namespace rsma_mcu