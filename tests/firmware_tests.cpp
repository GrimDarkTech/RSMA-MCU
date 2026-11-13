#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "rsma_mcu/firmware_format.h"
#include "rsma_mcu/firmware_validator.h"
#include "rsma_mcu/hitl_protocol.h"

namespace {

using namespace rsma_mcu;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

FirmwarePackage make_package()
{
    FirmwarePackage package;
    package.manifest.boardId = "RSMA-STM32F103";
    package.manifest.flashSize = 64 * 1024;
    package.manifest.ramSize = 20 * 1024;
    package.manifest.entryPointName = "reset";
    package.manifest.requiredPeripherals = {"GPIO", "SYSTICK"};
    package.bytecode.entryPoint = 0;
    package.bytecode.instructions = {
        {Opcode::HalCall, {1, 0, 13, 1}},
        {Opcode::HalCall, {2, 0, 13, 1}},
        {Opcode::Wait, {100, 0, 0, 0}},
        {Opcode::Jump, {1, 0, 0, 0}},
    };
    package.additionalSections.push_back({
        SectionType::Symbols,
        SectionFlags::ReadOnly,
        Bytes{'r', 'e', 's', 'e', 't'},
    });
    return package;
}

void test_round_trip(const std::filesystem::path& vectorDirectory)
{
    const auto package = make_package();
    require(validate_firmware(package).valid, "source package should validate");

    const auto bytes = serialize_firmware(package);
    require(bytes.size() > kHeaderSize, "serialized package should contain payload");

    const auto decoded = deserialize_firmware(bytes);
    require(decoded.manifest.boardId == package.manifest.boardId, "board id did not round-trip");
    require(decoded.manifest.requiredPeripherals == package.manifest.requiredPeripherals,
        "peripheral list did not round-trip");
    require(decoded.bytecode.instructions.size() == package.bytecode.instructions.size(),
        "instruction count did not round-trip");
    require(decoded.bytecode.instructions[0].operands == Bytes({1, 0, 13, 1}),
        "HAL_CALL operand layout did not round-trip");
    require(decoded.additionalSections.size() == 1, "additional section did not round-trip");

    std::filesystem::create_directories(vectorDirectory);
    const auto vectorPath = vectorDirectory / "minimal-valid.rsfw";
    write_firmware(package, vectorPath);
    require(read_firmware(vectorPath).bytecode.entryPoint == 0, "file round-trip failed");
}

void test_rejects_corruption()
{
    const auto package = make_package();
    const auto original = serialize_firmware(package);

    auto invalidMagic = original;
    invalidMagic[0] = 'X';
    require(!validate_firmware_bytes(invalidMagic).valid, "invalid magic should be rejected");

    auto invalidHeaderChecksum = original;
    invalidHeaderChecksum[52] ^= 0x01;
    require(!validate_firmware_bytes(invalidHeaderChecksum).valid,
        "invalid header checksum should be rejected");

    auto invalidPayload = original;
    invalidPayload.back() ^= 0x01;
    require(!validate_firmware_bytes(invalidPayload).valid,
        "invalid payload checksum should be rejected");

    auto invalidVersion = original;
    invalidVersion[6] = 2;
    require(!validate_firmware_bytes(invalidVersion).valid,
        "unsupported format version should be rejected");

    auto invalidEntryPoint = original;
    invalidEntryPoint[32] = 0xFF;
    require(!validate_firmware_bytes(invalidEntryPoint).valid,
        "invalid entry point should be rejected");
}

void test_rejects_invalid_package()
{
    auto package = make_package();
    package.bytecode.entryPoint = 100;

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::Yield, {}};
    require(validate_firmware(package).valid, "YIELD without operands should be accepted");

    package.bytecode.instructions[0] = {Opcode::Yield, {1}};
    require(!validate_firmware(package).valid, "YIELD operands should be rejected");
    require(!validate_firmware(package).valid, "invalid entry point should be rejected");

    package = make_package();
    package.manifest.boardId.clear();
    require(!validate_firmware(package).valid, "empty board id should be rejected");

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::Nop, {1}};
    require(!validate_firmware(package).valid, "invalid NOP operands should be rejected");

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::HalCall, {0xFF, 0x00, 13, 1}};
    require(!validate_firmware(package).valid, "unknown HAL service should be rejected");

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::HalCall, {1, 0, 13, 2}};
    require(!validate_firmware(package).valid, "invalid pin mode should be rejected");

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::Load, {0, 1}};
    require(!validate_firmware(package).valid, "LOAD slot outside VM memory should be rejected");

    package = make_package();
    package.bytecode.instructions[0] = {Opcode::HalCall, {6, 0, 7, 13}};
    require(validate_firmware(package).valid, "packed ConfigurePin HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::PwmWrite), 0, 7, 128}};
    require(validate_firmware(package).valid, "PWM HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartBegin), 0, 0, 4}};
    require(validate_firmware(package).valid, "UART begin HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartAvailable), 0, 3, 0}};
    require(validate_firmware(package).valid, "UART available HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartRead), 0, 0, 0}};
    require(validate_firmware(package).valid, "UART read HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartWrite), 0, 0, 0}};
    require(validate_firmware(package).valid, "UART write HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::AnalogRead), 0, 4, 0}};
    require(validate_firmware(package).valid, "ADC read HAL call should be accepted");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartBegin), 0, 4, 0}};
    require(!validate_firmware(package).valid, "UART port outside range should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartBegin), 0, 0, 5}};
    require(!validate_firmware(package).valid, "UART baud preset outside range should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::UartRead), 0, 0, 1}};
    require(!validate_firmware(package).valid, "UART read reserved value should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall, {13, 0, 7, 128}};
    require(!validate_firmware(package).valid, "unknown HAL service should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall,
        {static_cast<Byte>(HalService::AnalogRead), 0, 4, 1}};
    require(!validate_firmware(package).valid, "ADC reserved value should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall, {6, 0, 7, 0x80}};
    require(!validate_firmware(package).valid, "ConfigurePin reserved bits should be rejected");

    package.bytecode.instructions[0] = {Opcode::HalCall, {6, 0, 7, 0x06}};
    require(!validate_firmware(package).valid, "ConfigurePin invalid pull mode should be rejected");

    package.bytecode.instructions[0] = {Opcode::AttachIrq, {7, 3, 2, 0, 0, 0}};
    require(validate_firmware(package).valid, "valid ATTACH_IRQ should be accepted");

    package.bytecode.instructions[0] = {Opcode::AttachIrq, {7, 0, 2, 0, 0, 0}};
    require(!validate_firmware(package).valid, "ATTACH_IRQ without an edge should be rejected");

    package.bytecode.instructions[0] = {Opcode::AttachIrq, {7, 1, 0xFF, 0xFF, 0, 0}};
    require(!validate_firmware(package).valid, "ATTACH_IRQ outside bytecode should be rejected");

    package.bytecode.instructions[0] = {Opcode::ReturnIrq, {1}};
    require(!validate_firmware(package).valid, "RETURN_IRQ operands should be rejected");
}

void test_hitl_frame_round_trip_and_crc()
{
    HitlFrame frame;
    frame.type = HitlMessageType::Data;
    frame.flags = 3;
    frame.sequence = 0x10203040u;
    frame.timestampMilliseconds = 1700000000123ull;
    frame.payload = {1, 2, 3, 0xFF};

    const auto bytes = encode_hitl_frame(frame);
    require(bytes.size() == kHitlHeaderSize + frame.payload.size() + kHitlChecksumSize,
        "HITL frame has unexpected wire size");
    require(bytes[0] == 'R' && bytes[1] == 'S' && bytes[2] == 'M' && bytes[3] == 'H',
        "HITL magic does not match wire contract");

    HitlFrame decoded;
    std::string error;
    require(decode_hitl_frame(bytes, decoded, error), "valid HITL frame was rejected: " + error);
    require(decoded.version == frame.version, "HITL version did not round-trip");
    require(decoded.type == frame.type, "HITL message type did not round-trip");
    require(decoded.flags == frame.flags, "HITL flags did not round-trip");
    require(decoded.sequence == frame.sequence, "HITL sequence did not round-trip");
    require(decoded.timestampMilliseconds == frame.timestampMilliseconds,
        "HITL timestamp did not round-trip");
    require(decoded.payload == frame.payload, "HITL payload did not round-trip");

    auto corrupted = bytes;
    corrupted.back() ^= 0x01;
    require(!decode_hitl_frame(corrupted, decoded, error), "corrupt HITL CRC was accepted");
    require(error == "HITL frame CRC mismatch", "corrupt HITL frame returned an unexpected error");
}

void test_hitl_codec_matches_shared_golden_vector()
{
    const Bytes expected{
        0x52, 0x53, 0x4D, 0x48, 0x01, 0x05, 0x03, 0x00,
        0x40, 0x30, 0x20, 0x10, 0x7B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02,
        0x03, 0xFF, 0xE1, 0xE5, 0x78, 0xCE
    };
    HitlFrame frame;
    frame.type = HitlMessageType::Data;
    frame.flags = 3;
    frame.sequence = 0x10203040u;
    frame.timestampMilliseconds = 123;
    frame.payload = {1, 2, 3, 0xFF};

    require(encode_hitl_frame(frame) == expected, "C++ HITL codec differs from shared golden vector");
    HitlFrame decoded;
    std::string error;
    require(decode_hitl_frame(expected, decoded, error), "golden HITL vector was rejected: " + error);
    require(decoded.type == frame.type && decoded.flags == frame.flags &&
        decoded.sequence == frame.sequence && decoded.timestampMilliseconds == frame.timestampMilliseconds &&
        decoded.payload == frame.payload, "C++ decoder did not recover shared golden vector fields");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path vectorDirectory = argc > 1 ? argv[1] : "vectors";
        test_round_trip(vectorDirectory);
        test_rejects_corruption();
        test_rejects_invalid_package();
        test_hitl_frame_round_trip_and_crc();
        test_hitl_codec_matches_shared_golden_vector();
        std::cout << "All RSFW tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
