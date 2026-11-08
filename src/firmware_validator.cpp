#include "rsma_mcu/firmware_validator.h"

#include <limits>

namespace rsma_mcu {
namespace {

bool is_known_opcode(Opcode opcode)
{
    switch (opcode) {
    case Opcode::Nop:
    case Opcode::Halt:
    case Opcode::ConstI32:
    case Opcode::Load:
    case Opcode::Store:
    case Opcode::AddI32:
    case Opcode::SubI32:
    case Opcode::Jump:
    case Opcode::JumpIf:
    case Opcode::Call:
    case Opcode::Return:
    case Opcode::ReadTime:
    case Opcode::Wait:
    case Opcode::HalCall:
        return true;
    }
    return false;
}

} // namespace

ValidationResult validate_firmware(const FirmwarePackage& package)
{
    if (package.manifest.boardId.empty()) {
        return ValidationResult::failure("board id is empty");
    }
    if (package.manifest.flashSize == 0 || package.manifest.ramSize == 0) {
        return ValidationResult::failure("flash and RAM sizes must be non-zero");
    }
    if (package.manifest.entryPointName.empty()) {
        return ValidationResult::failure("entry point name is empty");
    }
    if (package.bytecode.instructions.empty()) {
        return ValidationResult::failure("bytecode has no instructions");
    }
    if (package.bytecode.entryPoint >= package.bytecode.instructions.size()) {
        return ValidationResult::failure("bytecode entry point is outside instruction list");
    }
    if (package.bytecode.instructions.size() > kMaxInstructionCount) {
        return ValidationResult::failure("bytecode instruction count exceeds implementation limit");
    }

    for (const auto& section : package.additionalSections) {
        if (section.type == SectionType::Manifest || section.type == SectionType::Bytecode) {
            return ValidationResult::failure("additional sections cannot duplicate required sections");
        }
        const auto rawFlags = static_cast<std::uint32_t>(section.flags);
        if ((rawFlags & ~kKnownSectionFlags) != 0) {
            return ValidationResult::failure("section contains unknown flags");
        }
        const auto sectionType = static_cast<std::uint32_t>(section.type);
        const auto knownSectionType = sectionType >= static_cast<std::uint32_t>(SectionType::ConstantPool) &&
            sectionType <= static_cast<std::uint32_t>(SectionType::Debug);
        if ((rawFlags & static_cast<std::uint32_t>(SectionFlags::Required)) != 0 && !knownSectionType) {
            return ValidationResult::failure("required section type is unknown");
        }
    }

    for (std::size_t i = 0; i < package.bytecode.instructions.size(); ++i) {
        const auto& instruction = package.bytecode.instructions[i];
        if (!is_known_opcode(instruction.opcode)) {
            return ValidationResult::failure("bytecode contains an unknown opcode");
        }
        if (instruction.opcode == Opcode::Nop || instruction.opcode == Opcode::Halt ||
            instruction.opcode == Opcode::AddI32 || instruction.opcode == Opcode::SubI32 ||
            instruction.opcode == Opcode::Return || instruction.opcode == Opcode::ReadTime) {
            if (!instruction.operands.empty()) {
                return ValidationResult::failure("operandless instruction has operands");
            }
        }
        if (instruction.opcode == Opcode::ConstI32 && instruction.operands.size() != 4) {
            return ValidationResult::failure("CONST_I32 must have four operand bytes");
        }
        if ((instruction.opcode == Opcode::Load || instruction.opcode == Opcode::Store) &&
            instruction.operands.size() != 2) {
            return ValidationResult::failure("LOAD and STORE must have a two-byte slot index");
        }
        if (instruction.opcode == Opcode::Load || instruction.opcode == Opcode::Store) {
            const auto slot = static_cast<std::uint16_t>(instruction.operands[0]) |
                (static_cast<std::uint16_t>(instruction.operands[1]) << 8);
            if (slot >= 256) {
                return ValidationResult::failure("LOAD/STORE slot index exceeds VM memory");
            }
        }
        if ((instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::JumpIf ||
             instruction.opcode == Opcode::Call || instruction.opcode == Opcode::Wait) &&
            instruction.operands.size() != 4) {
            return ValidationResult::failure("instruction must have four operand bytes");
        }
        if (instruction.opcode == Opcode::HalCall && instruction.operands.size() != 4) {
            return ValidationResult::failure("HAL_CALL must have four operand bytes");
        }
        if (instruction.opcode == Opcode::HalCall) {
            const auto service = static_cast<std::uint16_t>(instruction.operands[0]) |
                (static_cast<std::uint16_t>(instruction.operands[1]) << 8);
            const auto value = instruction.operands[3];
            if (service < static_cast<std::uint16_t>(HalService::PinMode) ||
                service > static_cast<std::uint16_t>(HalService::Millis)) {
                return ValidationResult::failure("HAL_CALL contains an unknown service ID");
            }
            if (service == static_cast<std::uint16_t>(HalService::PinMode) && value > 1) {
                return ValidationResult::failure("HAL PinMode value must be Input(0) or Output(1)");
            }
            if (service == static_cast<std::uint16_t>(HalService::DigitalWrite) && value > 1) {
                return ValidationResult::failure("HAL DigitalWrite value must be 0 or 1");
            }
        }
        if (instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::JumpIf ||
            instruction.opcode == Opcode::Call) {
            const auto target = static_cast<std::uint32_t>(instruction.operands[0]) |
                (static_cast<std::uint32_t>(instruction.operands[1]) << 8) |
                (static_cast<std::uint32_t>(instruction.operands[2]) << 16) |
                (static_cast<std::uint32_t>(instruction.operands[3]) << 24);
            if (target >= package.bytecode.instructions.size()) {
                return ValidationResult::failure("branch target is outside instruction list");
            }
        }
    }

    return ValidationResult::success();
}

ValidationResult validate_firmware_bytes(const Bytes& bytes)
{
    try {
        (void)deserialize_firmware(bytes);
        return ValidationResult::success();
    } catch (const FirmwareError& error) {
        return ValidationResult::failure(error.what());
    }
}

} // namespace rsma_mcu
