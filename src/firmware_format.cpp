#include "rsma_mcu/firmware_format.h"
#include "rsma_mcu/firmware_validator.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rsma_mcu {
namespace {

constexpr std::array<Byte, 4> kFirmwareMagic{'R', 'S', 'F', 'W'};
constexpr std::array<Byte, 4> kManifestMagic{'M', 'A', 'N', '1'};
constexpr std::array<Byte, 4> kBytecodeMagic{'B', 'C', 'O', 'D'};

class BufferWriter
{
public:
    void byte(Byte value) { data_.push_back(value); }

    void bytes(const Bytes& values)
    {
        data_.insert(data_.end(), values.begin(), values.end());
    }

    void string_bytes(const std::string& value)
    {
        data_.insert(data_.end(), value.begin(), value.end());
    }

    void u16(std::uint16_t value)
    {
        byte(static_cast<Byte>(value));
        byte(static_cast<Byte>(value >> 8));
    }

    void u32(std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i) {
            byte(static_cast<Byte>(value >> (i * 8)));
        }
    }

    void u64(std::uint64_t value)
    {
        for (unsigned i = 0; i < 8; ++i) {
            byte(static_cast<Byte>(value >> (i * 8)));
        }
    }

    Bytes take() && { return std::move(data_); }

private:
    Bytes data_;
};

class BufferReader
{
public:
    explicit BufferReader(const Bytes& data) : data_(data) {}

    std::size_t position() const { return position_; }

    void require(std::size_t count) const
    {
        if (count > data_.size() - position_) {
            throw FirmwareError("Unexpected end of firmware section");
        }
    }

    Byte byte()
    {
        require(1);
        return data_[position_++];
    }

    std::uint16_t u16()
    {
        const auto a = byte();
        const auto b = byte();
        return static_cast<std::uint16_t>(a | (static_cast<std::uint16_t>(b) << 8));
    }

    std::uint32_t u32()
    {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(byte()) << (i * 8);
        }
        return value;
    }

    std::uint64_t u64()
    {
        std::uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(byte()) << (i * 8);
        }
        return value;
    }

    Bytes bytes(std::size_t count)
    {
        require(count);
        Bytes result(data_.begin() + static_cast<std::ptrdiff_t>(position_),
            data_.begin() + static_cast<std::ptrdiff_t>(position_ + count));
        position_ += count;
        return result;
    }

    std::string string_bytes(std::size_t count)
    {
        const auto value = bytes(count);
        return {value.begin(), value.end()};
    }

private:
    const Bytes& data_;
    std::size_t position_ = 0;
};

std::uint16_t checked_u16(std::size_t value, const char* field)
{
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw FirmwareError(std::string(field) + " is too large");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint32_t checked_u32(std::size_t value, const char* field)
{
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw FirmwareError(std::string(field) + " is too large");
    }
    return static_cast<std::uint32_t>(value);
}

void expect_magic(BufferReader& reader, const std::array<Byte, 4>& magic, const char* name)
{
    const auto actual = reader.bytes(magic.size());
    if (!std::equal(actual.begin(), actual.end(), magic.begin())) {
        throw FirmwareError(std::string("Invalid ") + name + " magic");
    }
}

std::uint32_t crc32(const Byte* data, std::size_t size)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

std::uint32_t crc32(const Bytes& data)
{
    return crc32(data.data(), data.size());
}

void write_at(Bytes& data, std::size_t offset, std::uint32_t value)
{
    if (offset + 4 > data.size()) {
        throw FirmwareError("Internal firmware buffer write overflow");
    }
    for (unsigned i = 0; i < 4; ++i) {
        data[offset + i] = static_cast<Byte>(value >> (i * 8));
    }
}

void write_at(Bytes& data, std::size_t offset, std::uint64_t value)
{
    if (offset + 8 > data.size()) {
        throw FirmwareError("Internal firmware buffer write overflow");
    }
    for (unsigned i = 0; i < 8; ++i) {
        data[offset + i] = static_cast<Byte>(value >> (i * 8));
    }
}

std::uint16_t read_u16_at(const Bytes& data, std::size_t offset)
{
    if (offset + 2 > data.size()) {
        throw FirmwareError("Firmware header is truncated");
    }
    return static_cast<std::uint16_t>(data[offset] | (static_cast<std::uint16_t>(data[offset + 1]) << 8));
}

std::uint32_t read_u32_at(const Bytes& data, std::size_t offset)
{
    if (offset + 4 > data.size()) {
        throw FirmwareError("Firmware header is truncated");
    }
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(data[offset + i]) << (i * 8);
    }
    return value;
}

std::uint64_t read_u64_at(const Bytes& data, std::size_t offset)
{
    if (offset + 8 > data.size()) {
        throw FirmwareError("Firmware header is truncated");
    }
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[offset + i]) << (i * 8);
    }
    return value;
}

FirmwareHeaderView read_header(const Bytes& data)
{
    if (data.size() < kHeaderSize) {
        throw FirmwareError("Firmware is smaller than the fixed header");
    }
    if (!std::equal(kFirmwareMagic.begin(), kFirmwareMagic.end(), data.begin())) {
        throw FirmwareError("Invalid RSFW magic");
    }

    FirmwareHeaderView header;
    header.headerSize = read_u16_at(data, 4);
    header.formatMajor = read_u16_at(data, 6);
    header.formatMinor = read_u16_at(data, 8);
    header.vmVersion = read_u16_at(data, 10);
    header.halVersion = read_u16_at(data, 12);
    header.flags = read_u32_at(data, 16);
    header.sectionCount = read_u32_at(data, 20);
    header.sectionTableOffset = read_u64_at(data, 24);
    header.entryPoint = read_u32_at(data, 32);
    header.entryPointSection = read_u32_at(data, 36);
    header.totalSize = read_u64_at(data, 40);
    header.payloadChecksum = read_u32_at(data, 48);
    header.headerChecksum = read_u32_at(data, 52);
    if (read_u16_at(data, 14) != 0 || read_u64_at(data, 56) != 0) {
        throw FirmwareError("RSFW header reserved fields must be zero");
    }
    return header;
}

struct SectionView
{
    SectionType type;
    SectionFlags flags;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t checksum;
};

std::vector<SectionView> read_sections(const Bytes& data, const FirmwareHeaderView& header)
{
    if (header.headerSize != kHeaderSize) {
        throw FirmwareError("Unsupported RSFW header size");
    }
    if (header.sectionCount == 0) {
        throw FirmwareError("Firmware has no sections");
    }
    if (header.sectionCount > kMaxSectionCount) {
        throw FirmwareError("Firmware section count exceeds implementation limit");
    }
    if (header.sectionCount > (std::numeric_limits<std::size_t>::max() - header.sectionTableOffset) / kSectionEntrySize) {
        throw FirmwareError("Section table size overflows");
    }
    const auto tableEnd = static_cast<std::size_t>(header.sectionTableOffset + header.sectionCount * kSectionEntrySize);
    if (header.sectionTableOffset < header.headerSize || tableEnd > data.size()) {
        throw FirmwareError("Section table is outside the firmware");
    }

    std::vector<SectionView> sections;
    sections.reserve(header.sectionCount);
    for (std::uint32_t i = 0; i < header.sectionCount; ++i) {
        const auto offset = static_cast<std::size_t>(header.sectionTableOffset + i * kSectionEntrySize);
        const auto type = read_u32_at(data, offset);
        const auto flags = read_u32_at(data, offset + 4);
        const auto payloadOffset = read_u64_at(data, offset + 8);
        const auto payloadSize = read_u64_at(data, offset + 16);
        const auto checksum = read_u32_at(data, offset + 24);
        if (read_u32_at(data, offset + 28) != 0) {
            throw FirmwareError("Section reserved field must be zero");
        }

        if (payloadOffset > data.size() || payloadSize > data.size() - payloadOffset) {
            throw FirmwareError("Section payload is outside the firmware");
        }
        if (payloadOffset < tableEnd) {
            throw FirmwareError("Section payload overlaps the section table");
        }
        sections.push_back({static_cast<SectionType>(type), static_cast<SectionFlags>(flags), payloadOffset, payloadSize, checksum});
    }

    auto ranges = sections;
    std::sort(ranges.begin(), ranges.end(), [](const SectionView& left, const SectionView& right) {
        return left.offset < right.offset;
    });
    for (std::size_t i = 1; i < ranges.size(); ++i) {
        const auto previousEnd = ranges[i - 1].offset + ranges[i - 1].size;
        if (ranges[i].offset < previousEnd) {
            throw FirmwareError("Firmware sections overlap");
        }
    }
    return sections;
}

Bytes section_data(const Bytes& firmware, const SectionView& section)
{
    return {firmware.begin() + static_cast<std::ptrdiff_t>(section.offset),
        firmware.begin() + static_cast<std::ptrdiff_t>(section.offset + section.size)};
}

} // namespace

Bytes encode_manifest(const FirmwareManifest& manifest)
{
    BufferWriter writer;
    writer.bytes(Bytes{kManifestMagic.begin(), kManifestMagic.end()});
    writer.u16(1);
    writer.u16(0);
    writer.u32(manifest.flashSize);
    writer.u32(manifest.ramSize);
    writer.u16(checked_u16(manifest.boardId.size(), "board id"));
    writer.u16(checked_u16(manifest.entryPointName.size(), "entry point name"));
    writer.u16(checked_u16(manifest.requiredPeripherals.size(), "peripheral count"));
    writer.u16(0);
    writer.string_bytes(manifest.boardId);
    writer.string_bytes(manifest.entryPointName);
    for (const auto& peripheral : manifest.requiredPeripherals) {
        writer.u16(checked_u16(peripheral.size(), "peripheral name"));
        writer.string_bytes(peripheral);
    }
    return std::move(writer).take();
}

FirmwareManifest decode_manifest(const Bytes& data)
{
    BufferReader reader(data);
    expect_magic(reader, kManifestMagic, "manifest");
    if (reader.u16() != 1) {
        throw FirmwareError("Unsupported manifest version");
    }
    reader.u16();

    FirmwareManifest manifest;
    manifest.flashSize = reader.u32();
    manifest.ramSize = reader.u32();
    const auto boardSize = reader.u16();
    const auto entrySize = reader.u16();
    const auto peripheralCount = reader.u16();
    reader.u16();
    manifest.boardId = reader.string_bytes(boardSize);
    manifest.entryPointName = reader.string_bytes(entrySize);
    manifest.requiredPeripherals.reserve(peripheralCount);
    for (std::uint16_t i = 0; i < peripheralCount; ++i) {
        manifest.requiredPeripherals.push_back(reader.string_bytes(reader.u16()));
    }
    if (reader.position() != data.size()) {
        throw FirmwareError("Manifest contains trailing bytes");
    }
    return manifest;
}

Bytes encode_bytecode(const BytecodeProgram& program)
{
    if (program.instructions.size() > kMaxInstructionCount) {
        throw FirmwareError("Bytecode instruction count exceeds implementation limit");
    }
    if (program.entryPoint >= program.instructions.size() && !program.instructions.empty()) {
        throw FirmwareError("Bytecode entry point is outside instruction list");
    }
    BufferWriter writer;
    writer.bytes(Bytes{kBytecodeMagic.begin(), kBytecodeMagic.end()});
    writer.u16(kBytecodeVersion);
    writer.u16(0);
    writer.u32(program.entryPoint);
    writer.u32(checked_u32(program.instructions.size(), "instruction count"));
    for (const auto& instruction : program.instructions) {
        writer.byte(static_cast<Byte>(instruction.opcode));
        writer.byte(0);
        writer.u16(0);
        writer.u32(checked_u32(instruction.operands.size(), "instruction operands"));
        writer.bytes(instruction.operands);
    }
    return std::move(writer).take();
}

BytecodeProgram decode_bytecode(const Bytes& data)
{
    BufferReader reader(data);
    expect_magic(reader, kBytecodeMagic, "bytecode");
    if (reader.u16() != kBytecodeVersion) {
        throw FirmwareError("Unsupported bytecode version");
    }
    reader.u16();
    BytecodeProgram program;
    program.entryPoint = reader.u32();
    const auto instructionCount = reader.u32();
    if (instructionCount > kMaxInstructionCount) {
        throw FirmwareError("Bytecode instruction count exceeds implementation limit");
    }
    if (instructionCount > (data.size() - reader.position()) / 8) {
        throw FirmwareError("Bytecode instruction count exceeds available data");
    }
    program.instructions.reserve(instructionCount);
    for (std::uint32_t i = 0; i < instructionCount; ++i) {
        const auto opcode = static_cast<Opcode>(reader.byte());
        reader.byte();
        reader.u16();
        const auto operandSize = reader.u32();
        program.instructions.push_back({opcode, reader.bytes(operandSize)});
    }
    if (reader.position() != data.size()) {
        throw FirmwareError("Bytecode contains trailing bytes");
    }
    return program;
}

Bytes serialize_firmware(const FirmwarePackage& package)
{
    const auto validation = validate_firmware(package);
    if (!validation.valid) {
        throw FirmwareError("Cannot serialize invalid firmware: " + validation.error);
    }

    std::vector<FirmwareSection> sections;
    sections.push_back({SectionType::Manifest, SectionFlags::Required | SectionFlags::ReadOnly, encode_manifest(package.manifest)});
    sections.push_back({SectionType::Bytecode, SectionFlags::Required | SectionFlags::ReadOnly, encode_bytecode(package.bytecode)});
    sections.insert(sections.end(), package.additionalSections.begin(), package.additionalSections.end());

    if (sections.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw FirmwareError("Too many firmware sections");
    }
    if (sections.size() > kMaxSectionCount) {
        throw FirmwareError("Firmware section count exceeds implementation limit");
    }

    const auto tableOffset = kHeaderSize;
    const auto payloadOffset = tableOffset + sections.size() * kSectionEntrySize;
    std::size_t totalSize = payloadOffset;
    for (const auto& section : sections) {
        if (section.data.size() > std::numeric_limits<std::size_t>::max() - totalSize) {
            throw FirmwareError("Firmware size overflows");
        }
        totalSize += section.data.size();
    }
    if (totalSize > kMaxFirmwareSize) {
        throw FirmwareError("Firmware package exceeds implementation size limit");
    }

    BufferWriter header;
    header.bytes(Bytes{kFirmwareMagic.begin(), kFirmwareMagic.end()});
    header.u16(kHeaderSize);
    header.u16(kFormatMajor);
    header.u16(kFormatMinor);
    header.u16(kVmVersion);
    header.u16(kHalVersion);
    header.u16(0);
    header.u32(package.flags);
    header.u32(checked_u32(sections.size(), "section count"));
    header.u64(tableOffset);
    header.u32(package.bytecode.entryPoint);
    header.u32(static_cast<std::uint32_t>(SectionType::Bytecode));
    header.u64(totalSize);
    header.u32(0);
    header.u32(0);
    header.u64(0);
    auto result = std::move(header).take();

    BufferWriter table;
    std::size_t currentOffset = payloadOffset;
    Bytes payload;
    for (const auto& section : sections) {
        table.u32(static_cast<std::uint32_t>(section.type));
        table.u32(static_cast<std::uint32_t>(section.flags));
        table.u64(currentOffset);
        table.u64(section.data.size());
        table.u32(crc32(section.data));
        table.u32(0);
        payload.insert(payload.end(), section.data.begin(), section.data.end());
        currentOffset += section.data.size();
    }
    const auto tableBytes = std::move(table).take();
    result.insert(result.end(), tableBytes.begin(), tableBytes.end());
    result.insert(result.end(), payload.begin(), payload.end());

    write_at(result, 48, crc32(payload));
    write_at(result, 52, 0u);
    write_at(result, 52, crc32(result.data(), kHeaderSize));
    return result;
}

FirmwarePackage deserialize_firmware(const Bytes& bytes)
{
    if (bytes.size() > kMaxFirmwareSize) {
        throw FirmwareError("Firmware package exceeds implementation size limit");
    }
    const auto header = read_header(bytes);
    if (header.formatMajor != kFormatMajor || header.formatMinor > kFormatMinor) {
        throw FirmwareError("Unsupported RSFW format version");
    }
    if (header.vmVersion != kVmVersion || header.halVersion != kHalVersion) {
        throw FirmwareError("Unsupported RSFW VM or HAL version");
    }
    if (header.totalSize != bytes.size()) {
        throw FirmwareError("RSFW total size does not match file size");
    }

    auto headerForChecksum = Bytes(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
    write_at(headerForChecksum, 52, 0u);
    if (crc32(headerForChecksum) != header.headerChecksum) {
        throw FirmwareError("RSFW header checksum mismatch");
    }

    const auto sections = read_sections(bytes, header);
    Bytes payload;
    FirmwarePackage package;
    package.flags = header.flags;
    bool hasManifest = false;
    bool hasBytecode = false;
    for (const auto& section : sections) {
        const auto data = section_data(bytes, section);
        if (crc32(data) != section.checksum) {
            throw FirmwareError("RSFW section checksum mismatch");
        }
        const auto rawFlags = static_cast<std::uint32_t>(section.flags);
        if ((rawFlags & ~kKnownSectionFlags) != 0) {
            throw FirmwareError("Section contains unknown flags");
        }
        payload.insert(payload.end(), data.begin(), data.end());
        if (section.type == SectionType::Manifest) {
            if (hasManifest) throw FirmwareError("RSFW contains duplicate manifest section");
            package.manifest = decode_manifest(data);
            hasManifest = true;
        } else if (section.type == SectionType::Bytecode) {
            if (hasBytecode) throw FirmwareError("RSFW contains duplicate bytecode section");
            package.bytecode = decode_bytecode(data);
            hasBytecode = true;
        } else {
            if ((rawFlags & static_cast<std::uint32_t>(SectionFlags::Required)) != 0 &&
                static_cast<std::uint32_t>(section.type) > static_cast<std::uint32_t>(SectionType::Debug)) {
                throw FirmwareError("Firmware requires an unknown section type");
            }
            package.additionalSections.push_back({section.type, section.flags, data});
        }
    }
    if (crc32(payload) != header.payloadChecksum) {
        throw FirmwareError("RSFW payload checksum mismatch");
    }
    if (!hasManifest || !hasBytecode) {
        throw FirmwareError("RSFW is missing a required section");
    }
    if (header.entryPointSection != static_cast<std::uint32_t>(SectionType::Bytecode) ||
        header.entryPoint != package.bytecode.entryPoint) {
        throw FirmwareError("RSFW entry point metadata mismatch");
    }
    const auto validation = validate_firmware(package);
    if (!validation.valid) {
        throw FirmwareError("Invalid RSFW package: " + validation.error);
    }
    return package;
}

void write_firmware(const FirmwarePackage& package, const std::filesystem::path& path)
{
    const auto bytes = serialize_firmware(package);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw FirmwareError("Cannot open firmware output: " + path.string());
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw FirmwareError("Cannot write firmware output: " + path.string());
    }
}

FirmwarePackage read_firmware(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw FirmwareError("Cannot open firmware input: " + path.string());
    }
    const auto begin = std::istreambuf_iterator<char>(stream);
    const auto end = std::istreambuf_iterator<char>();
    Bytes bytes;
    for (auto it = begin; it != end; ++it) {
        bytes.push_back(static_cast<Byte>(static_cast<unsigned char>(*it)));
    }
    return deserialize_firmware(bytes);
}

std::string section_type_name(SectionType type)
{
    switch (type) {
    case SectionType::Manifest: return "Manifest";
    case SectionType::Bytecode: return "Bytecode";
    case SectionType::ConstantPool: return "ConstantPool";
    case SectionType::MemoryImage: return "MemoryImage";
    case SectionType::BoardRequirements: return "BoardRequirements";
    case SectionType::Symbols: return "Symbols";
    case SectionType::Debug: return "Debug";
    }
    return "Unknown";
}

} // namespace rsma_mcu
