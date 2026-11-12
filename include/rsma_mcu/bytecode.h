#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rsma_mcu {

enum class Opcode : std::uint8_t
{
    Nop = 0x00,
    Halt = 0x01,
    ConstI32 = 0x02,
    Load = 0x03,
    Store = 0x04,
    AddI32 = 0x05,
    SubI32 = 0x06,
    Jump = 0x07,
    JumpIf = 0x08,
    Call = 0x09,
    Return = 0x0A,
    ReadTime = 0x0B,
    Wait = 0x0C,
    HalCall = 0x0D,
    AttachIrq = 0x0E,
    EnableIrq = 0x0F,
    DisableIrq = 0x10,
    ReturnIrq = 0x11,
    CmpEqI32 = 0x12,
    CmpNeI32 = 0x13,
    CmpLtI32 = 0x14,
    CmpLeI32 = 0x15,
    CmpGtI32 = 0x16,
    CmpGeI32 = 0x17,
    GlobalLoad = 0x18,
    GlobalStore = 0x19,
};

constexpr std::uint16_t kBytecodeVersion = 1;
constexpr std::size_t kInstructionHeaderSize = 4;
constexpr std::uint32_t kMaxInstructionCount = 1'000'000;

enum class HalService : std::uint16_t
{
    PinMode = 1,
    DigitalWrite = 2,
    DigitalRead = 3,
    DelayMs = 4,
    Millis = 5,
    ConfigurePin = 6,
    PwmWrite = 7,
    UartBegin = 8,
    UartAvailable = 9,
    UartRead = 10,
    UartWrite = 11,
};

constexpr std::string_view opcode_name(Opcode opcode)
{
    switch (opcode) {
    case Opcode::Nop: return "NOP";
    case Opcode::Halt: return "HALT";
    case Opcode::ConstI32: return "CONST_I32";
    case Opcode::Load: return "LOAD";
    case Opcode::Store: return "STORE";
    case Opcode::AddI32: return "ADD_I32";
    case Opcode::SubI32: return "SUB_I32";
    case Opcode::Jump: return "JUMP";
    case Opcode::JumpIf: return "JUMP_IF";
    case Opcode::Call: return "CALL";
    case Opcode::Return: return "RETURN";
    case Opcode::ReadTime: return "READ_TIME";
    case Opcode::Wait: return "WAIT";
    case Opcode::HalCall: return "HAL_CALL";
    case Opcode::AttachIrq: return "ATTACH_IRQ";
    case Opcode::EnableIrq: return "ENABLE_IRQ";
    case Opcode::DisableIrq: return "DISABLE_IRQ";
    case Opcode::ReturnIrq: return "RETURN_IRQ";
    case Opcode::CmpEqI32: return "CMP_EQ_I32";
    case Opcode::CmpNeI32: return "CMP_NE_I32";
    case Opcode::CmpLtI32: return "CMP_LT_I32";
    case Opcode::CmpLeI32: return "CMP_LE_I32";
    case Opcode::CmpGtI32: return "CMP_GT_I32";
    case Opcode::CmpGeI32: return "CMP_GE_I32";
    case Opcode::GlobalLoad: return "GLOBAL_LOAD";
    case Opcode::GlobalStore: return "GLOBAL_STORE";
    }
    return "UNKNOWN";
}

} // namespace rsma_mcu
