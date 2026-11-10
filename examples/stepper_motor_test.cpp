#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "rsma_mcu/firmware_format.h"

namespace {

using namespace rsma_mcu;

Bytes u32(std::uint32_t value)
{
    return Bytes{
        static_cast<Byte>(value),
        static_cast<Byte>(value >> 8),
        static_cast<Byte>(value >> 16),
        static_cast<Byte>(value >> 24),
    };
}

Bytes hal(HalService service, std::uint8_t pin, std::uint8_t value)
{
    const auto id = static_cast<std::uint16_t>(service);
    return Bytes{
        static_cast<Byte>(id),
        static_cast<Byte>(id >> 8),
        pin,
        value,
    };
}

void append_phase(std::vector<BytecodeInstruction>& instructions,
                  std::uint8_t channel1,
                  std::uint8_t channel2,
                  std::uint8_t channel3,
                  std::uint8_t channel4)
{
    instructions.push_back({Opcode::HalCall, hal(HalService::DigitalWrite, 10, channel1)});
    instructions.push_back({Opcode::HalCall, hal(HalService::DigitalWrite, 11, channel2)});
    instructions.push_back({Opcode::HalCall, hal(HalService::DigitalWrite, 12, channel3)});
    instructions.push_back({Opcode::HalCall, hal(HalService::DigitalWrite, 13, channel4)});
    instructions.push_back({Opcode::Wait, u32(100)});
}

FirmwarePackage make_stepper_motor_test_package()
{
    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        {Opcode::HalCall, hal(HalService::PinMode, 10, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 11, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 12, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 13, 1)},
    };

    // Virtual pins 10..13 map to the four L298N input channels.
    append_phase(package.bytecode.instructions, 0, 0, 0, 0);
    append_phase(package.bytecode.instructions, 1, 1, 0, 0);
    append_phase(package.bytecode.instructions, 0, 1, 1, 0);
    append_phase(package.bytecode.instructions, 0, 0, 1, 1);
    append_phase(package.bytecode.instructions, 1, 0, 0, 1);
    append_phase(package.bytecode.instructions, 0, 0, 0, 0);

    // Reverse sequence: phase 4 -> phase 3 -> phase 2 -> phase 1.
    append_phase(package.bytecode.instructions, 1, 0, 0, 1);
    append_phase(package.bytecode.instructions, 0, 0, 1, 1);
    append_phase(package.bytecode.instructions, 0, 1, 1, 0);
    append_phase(package.bytecode.instructions, 1, 1, 0, 0);
    append_phase(package.bytecode.instructions, 0, 0, 0, 0);

    // The first all-low phase starts at instruction 4.
    package.bytecode.instructions.push_back({Opcode::Jump, u32(4)});
    return package;
}

int usage()
{
    std::cerr << "Usage: rsma-stepper-motor-test <output.rsfw>\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return usage();

    try {
        write_firmware(make_stepper_motor_test_package(), argv[1]);
        std::cout << "Created " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Firmware error: " << error.what() << '\n';
        return 1;
    }
}