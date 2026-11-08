#include <cstdlib>
#include <iostream>
#include <string>

#include "rsma_mcu/firmware_format.h"

namespace {

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
}

int usage()
{
    std::cerr << "Usage:\n"
              << "  rsma-firmware-inspect <file.rsfw>\n"
              << "  rsma-firmware-inspect --create-blink <file.rsfw>\n"
              << "  rsma-firmware-inspect --create-button-irq <file.rsfw>\n";
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
