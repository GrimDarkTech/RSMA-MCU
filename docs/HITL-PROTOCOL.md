# RSMA HITL serial protocol, version 1

This document defines the byte stream shared by the C# host codec and the
portable C++ bridge codec. Multi-byte integers are unsigned little-endian.
Frames are concatenated directly; the fixed header and payload length make a
separate delimiter or escaping scheme unnecessary.

## Frame layout

| Offset | Size | Field | Version 1 value / meaning |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `RSMH` |
| 4 | 1 | Version | `1` |
| 5 | 1 | Message type | `1` Hello, `2` HelloAck, `3` Heartbeat, `4` HeartbeatAck, `5` Data, `6` SafeState |
| 6 | 2 | Flags | Reserved for negotiated capabilities; send zero unless defined by a capability |
| 8 | 4 | Sequence | Monotonically increasing per sender and connection |
| 12 | 8 | Timestamp | Sender monotonic milliseconds; not wall-clock time |
| 20 | 2 | Payload length | Number of bytes, maximum 65535 |
| 22 | N | Payload | Message-specific bytes |
| 22+N | 4 | CRC-32 | IEEE CRC-32, reflected polynomial `0xEDB88320`, init/xorout `0xFFFFFFFF` |

CRC covers offsets 4 through the final payload byte (version through payload),
excluding the four-byte magic and the CRC field itself. A frame's total size is
`26 + payload_length` bytes. Receivers reject unsupported versions/types,
truncation, inconsistent lengths, and CRC mismatch before dispatching payload.

## Session behavior

- Each direction maintains its own sequence. A receiver accepts only a sequence
  newer than the previous accepted sequence in that connection; reconnect resets
  the sequence state.
- Heartbeat timestamps are local monotonic milliseconds. A heartbeat or
  heartbeat-ack refreshes the receiver's timeout clock.
- On heartbeat timeout or an established-session disconnect, the host applies
  its configured safe state once and stops dispatching data until reconnect.
- The board bridge must independently apply its own safe outputs when its host
  heartbeat expires; transport loss must never leave a motor output latched.
- Payload schemas and hardware UART/USB selection are adapter-specific and must
  be versioned separately from this frame envelope.

`HitlFrameCodec` in the RSMA Unity project and `hitl_protocol.h` in `rsma-mcu`
implement the common envelope. The portable C++ codec is not itself an STM32
UART driver; board firmware must provide byte I/O and the independent watchdog.