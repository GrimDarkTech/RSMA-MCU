#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rsma_mcu/bytecode.h"

namespace rsma_mcu {

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;

constexpr std::uint16_t kFormatMajor = 1;
constexpr std::uint16_t kFormatMinor = 0;
constexpr std::uint16_t kVmVersion = 1;
constexpr std::uint16_t kHalVersion = 1;
constexpr std::size_t kHeaderSize = 64;
constexpr std::size_t kSectionEntrySize = 32;
constexpr std::uint32_t kMaxSectionCount = 64;
constexpr std::size_t kMaxFirmwareSize = 256u * 1024u * 1024u;

enum class SectionType : std::uint32_t
{
    Manifest = 1,
    Bytecode = 2,
    ConstantPool = 3,
    MemoryImage = 4,
    BoardRequirements = 5,
    Symbols = 6,
    Debug = 7,
};

enum class SectionFlags : std::uint32_t
{
    None = 0,
    Required = 1u << 0,
    ReadOnly = 1u << 1,
    DebugOnly = 1u << 2,
};

constexpr std::uint32_t kKnownSectionFlags =
    static_cast<std::uint32_t>(SectionFlags::Required) |
    static_cast<std::uint32_t>(SectionFlags::ReadOnly) |
    static_cast<std::uint32_t>(SectionFlags::DebugOnly);

constexpr SectionFlags operator|(SectionFlags left, SectionFlags right)
{
    return static_cast<SectionFlags>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

struct FirmwareManifest
{
    std::string boardId;
    std::uint32_t flashSize = 0;
    std::uint32_t ramSize = 0;
    std::string entryPointName;
    std::vector<std::string> requiredPeripherals;
};

struct BytecodeInstruction
{
    Opcode opcode = Opcode::Nop;
    Bytes operands;
};

struct BytecodeProgram
{
    std::vector<BytecodeInstruction> instructions;
    std::uint32_t entryPoint = 0;
};

struct FirmwareSection
{
    SectionType type = SectionType::Manifest;
    SectionFlags flags = SectionFlags::None;
    Bytes data;
};

struct FirmwarePackage
{
    std::uint32_t flags = 0;
    FirmwareManifest manifest;
    BytecodeProgram bytecode;
    std::vector<FirmwareSection> additionalSections;
};

struct FirmwareHeaderView
{
    std::uint16_t headerSize = 0;
    std::uint16_t formatMajor = 0;
    std::uint16_t formatMinor = 0;
    std::uint16_t vmVersion = 0;
    std::uint16_t halVersion = 0;
    std::uint32_t flags = 0;
    std::uint32_t sectionCount = 0;
    std::uint64_t sectionTableOffset = 0;
    std::uint32_t entryPoint = 0;
    std::uint32_t entryPointSection = 0;
    std::uint64_t totalSize = 0;
    std::uint32_t payloadChecksum = 0;
    std::uint32_t headerChecksum = 0;
};

class FirmwareError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

Bytes encode_manifest(const FirmwareManifest& manifest);
FirmwareManifest decode_manifest(const Bytes& data);

Bytes encode_bytecode(const BytecodeProgram& program);
BytecodeProgram decode_bytecode(const Bytes& data);

Bytes serialize_firmware(const FirmwarePackage& package);
FirmwarePackage deserialize_firmware(const Bytes& bytes);

void write_firmware(const FirmwarePackage& package, const std::filesystem::path& path);
FirmwarePackage read_firmware(const std::filesystem::path& path);

std::string section_type_name(SectionType type);

} // namespace rsma_mcu
