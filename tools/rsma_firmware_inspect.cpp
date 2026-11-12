#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

#include "rsma_mcu/firmware_format.h"

namespace {

std::uint16_t read_u16(const rsma_mcu::Bytes& data, std::size_t& offset)
{
    if (offset + 2 > data.size()) throw rsma_mcu::FirmwareError("Malformed symbols section");
    const auto value = static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
    offset += 2;
    return value;
}

std::uint32_t read_u32(const rsma_mcu::Bytes& data, std::size_t& offset)
{
    if (offset + 4 > data.size()) throw rsma_mcu::FirmwareError("Malformed symbols section");
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
        value |= static_cast<std::uint32_t>(data[offset++]) << (byte * 8);
    return value;
}

std::string read_string(const rsma_mcu::Bytes& data, std::size_t& offset)
{
    const auto length = read_u16(data, offset);
    if (offset + length > data.size()) throw rsma_mcu::FirmwareError("Malformed symbols section");
    const std::string value(data.begin() + static_cast<std::ptrdiff_t>(offset),
        data.begin() + static_cast<std::ptrdiff_t>(offset + length));
    offset += length;
    return value;
}

void print_symbols(const rsma_mcu::FirmwareSection& section)
{
    const auto& data = section.data;
    if (data.size() < 8 || data[0] != 'S' || data[1] != 'Y' || data[2] != 'M' || data[3] != '1')
        throw rsma_mcu::FirmwareError("Malformed symbols section header");
    std::size_t offset = 4;
    if (read_u16(data, offset) != 1) throw rsma_mcu::FirmwareError("Unsupported symbols section version");
    const auto count = read_u16(data, offset);
    std::cout << "Symbols: " << count << '\n';
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto address = read_u32(data, offset);
        const auto name = read_string(data, offset);
        std::cout << "  " << name << " = instruction " << address << '\n';
    }
    if (offset != data.size()) throw rsma_mcu::FirmwareError("Symbols section contains trailing bytes");
}

rsma_mcu::FirmwarePackage make_blink_package()
{
    using namespace rsma_mcu;

    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        {Opcode::HalCall, {0x01, 0x00, 0x0D, 0x01}},
        {Opcode::HalCall, {0x02, 0x00, 0x0D, 0x01}},
        {Opcode::Wait, {0xF4, 0x01, 0x00, 0x00}},
        {Opcode::HalCall, {0x02, 0x00, 0x0D, 0x00}},
        {Opcode::Wait, {0xF4, 0x01, 0x00, 0x00}},
        {Opcode::Jump, {0x01, 0x00, 0x00, 0x00}},
    };
    return package;
}

rsma_mcu::FirmwarePackage make_button_irq_package()
{
    using namespace rsma_mcu;

    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK", "IRQ"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        // PC13 is the active-low RollerSwitch input in the STM32F103 profile.
        {Opcode::HalCall, {0x06, 0x00, 0x0D, 0x02}}, // CONFIGURE_PIN(13, INPUT + PULL_UP)
        {Opcode::AttachIrq, {0x0D, 0x02, 0x05, 0x00, 0x00, 0x00}}, // falling -> handler 5
        {Opcode::EnableIrq, {}},
        {Opcode::Wait, {0xE8, 0x03, 0x00, 0x00}}, // 1000 ms
        {Opcode::Jump, {0x03, 0x00, 0x00, 0x00}},
        {Opcode::ConstI32, {0x01, 0x00, 0x00, 0x00}}, // event marker
        {Opcode::Store, {0x00, 0x00}},
        {Opcode::ReturnIrq, {}},
    };
    return package;
}

rsma_mcu::FirmwarePackage make_pwm_motor_test_package()
{
    using namespace rsma_mcu;

    constexpr auto hal = [](HalService service, std::uint8_t pin, std::uint8_t value) {
        const auto id = static_cast<std::uint16_t>(service);
        return Bytes{
            static_cast<Byte>(id), static_cast<Byte>(id >> 8), pin, value};
    };
    constexpr auto wait = [](std::uint32_t milliseconds) {
        return Bytes{
            static_cast<Byte>(milliseconds), static_cast<Byte>(milliseconds >> 8),
            static_cast<Byte>(milliseconds >> 16), static_cast<Byte>(milliseconds >> 24)};
    };
    constexpr auto jump = [](std::uint32_t instruction) {
        return Bytes{
            static_cast<Byte>(instruction), static_cast<Byte>(instruction >> 8),
            static_cast<Byte>(instruction >> 16), static_cast<Byte>(instruction >> 24)};
    };

    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK", "TIM"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        // Configure motor direction pins 10/11 and the shared PWM pin 13.
        {Opcode::HalCall, hal(HalService::PinMode, 10, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 11, 1)},
        {Opcode::HalCall, hal(HalService::PinMode, 13, 1)},

        // Stop, then run forward with duty 64/128/192/255.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)}, // 3
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)}, // 4
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},     // 5
        {Opcode::Wait, wait(500)},                               // 6
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 1)}, // 7
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)}, // 8
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},    // 9
        {Opcode::Wait, wait(500)},                               // 10
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},   // 11
        {Opcode::Wait, wait(500)},                               // 12
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},   // 13
        {Opcode::Wait, wait(500)},                               // 14
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 255)},   // 15
        {Opcode::Wait, wait(1000)},                              // 16
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},   // 17
        {Opcode::Wait, wait(500)},                               // 18
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},   // 19
        {Opcode::Wait, wait(500)},                               // 20
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},    // 21
        {Opcode::Wait, wait(500)},                               // 22
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},     // 23
        {Opcode::Wait, wait(500)},                               // 24

        // Stop, reverse direction, then run backward with the same PWM ramp.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)}, // 25
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 1)}, // 26
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},    // 27
        {Opcode::Wait, wait(500)},                               // 28
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},   // 29
        {Opcode::Wait, wait(500)},                               // 30
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},   // 31
        {Opcode::Wait, wait(500)},                               // 32
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 255)},   // 33
        {Opcode::Wait, wait(1000)},                              // 34
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 192)},   // 35
        {Opcode::Wait, wait(500)},                               // 36
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},   // 37
        {Opcode::Wait, wait(500)},                               // 38
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 64)},    // 39
        {Opcode::Wait, wait(500)},                               // 40
        {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},     // 41
        {Opcode::Wait, wait(500)},                               // 42

        // Leave both direction inputs low before starting the next cycle.
        {Opcode::HalCall, hal(HalService::DigitalWrite, 10, 0)}, // 43
        {Opcode::HalCall, hal(HalService::DigitalWrite, 11, 0)}, // 44
        {Opcode::Wait, wait(1000)},                               // 45
        {Opcode::Jump, jump(3)},                                  // 46
    };
    return package;
}

void print_package(const rsma_mcu::FirmwarePackage& package)
{
    using namespace rsma_mcu;

    std::cout << "Board: " << package.manifest.boardId << '\n';
    std::cout << "Flash: " << package.manifest.flashSize << " bytes\n";
    std::cout << "RAM: " << package.manifest.ramSize << " bytes\n";
    std::cout << "Entry: " << package.manifest.entryPointName
              << " (instruction " << package.bytecode.entryPoint << ")\n";
    std::cout << "Instructions: " << package.bytecode.instructions.size() << '\n';
    for (std::size_t i = 0; i < package.bytecode.instructions.size(); ++i) {
        const auto& instruction = package.bytecode.instructions[i];
        std::cout << "  [" << i << "] " << opcode_name(instruction.opcode)
                  << " (" << instruction.operands.size() << " operand bytes)\n";
    }
    std::cout << "Peripherals:";
    for (const auto& peripheral : package.manifest.requiredPeripherals) {
        std::cout << ' ' << peripheral;
    }
    std::cout << '\n';
    std::cout << "Additional sections: " << package.additionalSections.size() << '\n';
    for (const auto& section : package.additionalSections) {
        std::cout << "Section: " << section_type_name(section.type) << " (" << section.data.size() << " bytes)\n";
        if (section.type == SectionType::Symbols) print_symbols(section);
    }
}

int usage()
{
    std::cerr << "Usage:\n"
              << "  rsma-firmware-inspect <file.rsfw>\n"
              << "  rsma-firmware-inspect --create-blink <file.rsfw>\n"
              << "  rsma-firmware-inspect --create-button-irq <file.rsfw>\n"
              << "  rsma-firmware-inspect --create-pwm-motor-test <file.rsfw>\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        return usage();
    }

    try {
        if (argc == 3 && std::string(argv[1]) == "--create-blink") {
            const auto package = make_blink_package();
            rsma_mcu::write_firmware(package, argv[2]);
            std::cout << "Created " << argv[2] << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--create-button-irq") {
            const auto package = make_button_irq_package();
            rsma_mcu::write_firmware(package, argv[2]);
            std::cout << "Created " << argv[2] << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--create-pwm-motor-test") {
            const auto package = make_pwm_motor_test_package();
            rsma_mcu::write_firmware(package, argv[2]);
            std::cout << "Created " << argv[2] << '\n';
            return 0;
        }
        if (argc != 2) {
            return usage();
        }

        const auto package = rsma_mcu::read_firmware(argv[1]);
        print_package(package);
        return 0;
    } catch (const rsma_mcu::FirmwareError& error) {
        std::cerr << "Firmware error: " << error.what() << '\n';
        return 1;
    }
}
