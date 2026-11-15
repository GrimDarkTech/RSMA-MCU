#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rsma_mcu\firmware_format.h"
#include "rsma_mcu\firmware_validator.h"

namespace rsma_mcu {

enum class PinResetMode : std::uint8_t
{
    Input = 0,
    Output = 1,
};

enum class PinPullMode : std::uint8_t
{
    None = 0,
    PullUp = 1,
    PullDown = 2,
};

enum class PinOutputType : std::uint8_t
{
    PushPull = 0,
    OpenDrain = 1,
};

enum class PinCapability : std::uint16_t
{
    None = 0,
    DigitalInput = 1u << 0,
    DigitalOutput = 1u << 1,
    PullUp = 1u << 2,
    PullDown = 1u << 3,
    OpenDrain = 1u << 4,
    Analog = 1u << 5,
    Pwm = 1u << 6,
    ExternalInterrupt = 1u << 7,
};

constexpr PinCapability operator|(PinCapability left, PinCapability right)
{
    return static_cast<PinCapability>(static_cast<std::uint16_t>(left) |
        static_cast<std::uint16_t>(right));
}

constexpr bool has_capability(PinCapability value, PinCapability capability)
{
    return (static_cast<std::uint16_t>(value) & static_cast<std::uint16_t>(capability)) ==
        static_cast<std::uint16_t>(capability);
}

struct AlternateFunction
{
    std::string name;
    std::string route;
};

struct PhysicalRoute
{
    std::string portName;
    std::string pinName;
};

struct PinDefinition
{
    std::uint8_t virtualPin = 0;
    std::string portName;
    std::string pinName;
    PinResetMode resetMode = PinResetMode::Input;
    PinPullMode resetPullMode = PinPullMode::None;
    PinOutputType outputType = PinOutputType::PushPull;
    bool resetLatchLevel = false;
    PinCapability capabilities = PinCapability::None;
    std::vector<AlternateFunction> alternateFunctions;
    std::vector<PhysicalRoute> physicalRoutes;
};

struct BoardProfile
{
    std::string boardId;
    std::uint32_t flashSize = 0;
    std::uint32_t ramSize = 0;
    std::vector<std::string> supportedPeripherals;
    std::vector<PinDefinition> pins;
};

const BoardProfile* find_board_profile(const std::string& boardId);
ValidationResult validate_board_profile(const BoardProfile& profile);
ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const BoardProfile& profile);
ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const std::string& boardId);

} // namespace rsma_mcu