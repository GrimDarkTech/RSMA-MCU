#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

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

FirmwarePackage make_pwm_motor_test_package()
{
    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK", "TIM"};
    package.bytecode.entryPoint = 0;

    // The scene wiring uses:
    //   pin 10 -> motor driver IN1
    //   pin 11 -> motor driver IN2
    //   pin 13 -> shared PWM input
    package.bytecode.instructions = {
        {Opcode::HalCall, hal(HalService::PinMode, 10, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 11, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 13, 1)},

        // Stop before changing direction.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)},
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},
        {Opcode::Wait, u32(500)},

        // Forward: IN1=1, IN2=0. Ramp PWM up and down.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 1)},
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 255)},
        {Opcode::Wait, u32(1000)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},
        {Opcode::Wait, u32(500)},

        // Reverse: IN1=0, IN2=1. Repeat the same PWM ramp.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)},
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 1)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 255)},
        {Opcode::Wait, u32(1000)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},
        {Opcode::Wait, u32(500)},
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},
        {Opcode::Wait, u32(500)},

        // Leave the bridge stopped before restarting the sequence.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)},
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)},
        {Opcode::Wait, u32(1000)},
        {Opcode::Jump, u32(3)},
    };
    return package;
}

int usage()
{
    std::cerr << "Usage: rsma-pwm-motor-test <output.rsfw>\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        return usage();
    }

    try {
        write_firmware(make_pwm_motor_test_package(), argv[1]);
        std::cout << "Created " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Firmware error: " << error.what() << '\n';
        return 1;
    }
}