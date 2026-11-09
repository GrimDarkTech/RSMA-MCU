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

Bytes hal(HalService service, std::uint8_t port = 0, std::uint8_t value = 0)
{
    const auto id = static_cast<std::uint16_t>(service);
    return Bytes{
        static_cast<Byte>(id),
        static_cast<Byte>(id >> 8),
        port,
        value,
    };
}

FirmwarePackage make_uart_echo_package()
{
    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"UART"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        {Opcode::HalCall, hal(HalService::UartBegin, 0, 4)},
        {Opcode::HalCall, hal(HalService::UartAvailable)},
        {Opcode::JumpIf, u32(5)},
        {Opcode::Wait, u32(1)},
        {Opcode::Jump, u32(1)},
        {Opcode::HalCall, hal(HalService::UartRead)},
        {Opcode::Store, Bytes{0, 0}},
        {Opcode::Load, Bytes{0, 0}},
        {Opcode::HalCall, hal(HalService::UartWrite)},
        {Opcode::Jump, u32(1)},
    };
    return package;
}

int usage()
{
    std::cerr << "Usage: rsma-uart-echo <output.rsfw>\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        return usage();
    }

    try {
        write_firmware(make_uart_echo_package(), argv[1]);
        std::cout << "Created " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Firmware error: " << error.what() << '\n';
        return 1;
    }
}