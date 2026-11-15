#include "rsma_mcu\board_profiles.h"

#include <algorithm>
#include <unordered_set>

namespace rsma_mcu {
namespace {

const std::vector<BoardProfile>& profiles()
{
    static const std::vector<BoardProfile> value{
        {
            "RSMA-STM32F103",
            64 * 1024,
            20 * 1024,
            {"GPIO", "SYSTICK", "IRQ", "PWM", "UART", "ADC", "SPI", "I2C", "DMA", "EEPROM"},
            {
                {10, "PC", "10", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::PullUp |
                        PinCapability::PullDown | PinCapability::Analog | PinCapability::ExternalInterrupt,
                    {{"ADC", "ADC10"}, {"EXTI", "EXTI10"}}},
                {11, "PC", "11", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::PullUp |
                        PinCapability::PullDown | PinCapability::ExternalInterrupt,
                    {{"EXTI", "EXTI11"}}},
                {13, "PC", "13", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::Pwm,
                    {{"PWM", "TIM1_CH1"}}},
            },
        },
        {
            "RSMA-AVR-ATmega328P",
            32 * 1024,
            2 * 1024,
            {"GPIO", "SYSTICK", "IRQ", "PWM", "UART", "ADC", "SPI", "I2C", "EEPROM"},
            {
                {11, "PB", "3", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::Pwm,
                    {{"PWM", "TIM2_CH2"}}},
                {12, "PB", "4", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::ExternalInterrupt,
                    {{"EXTI", "EXTI4"}}},
                {13, "PB", "5", PinResetMode::Input, PinPullMode::None, PinOutputType::PushPull, false,
                    PinCapability::DigitalInput | PinCapability::DigitalOutput | PinCapability::Analog,
                    {{"ADC", "ADC0"}}},
            },
        },
    };
    return value;
}

bool supports(const BoardProfile& profile, const std::string& peripheral)
{
    return std::find(profile.supportedPeripherals.begin(), profile.supportedPeripherals.end(), peripheral) !=
        profile.supportedPeripherals.end();
}

ValidationResult validate_pin_definition(const PinDefinition& pin,
    std::unordered_set<std::uint8_t>& virtualPins,
    std::unordered_set<std::string>& physicalPins,
    std::unordered_set<std::string>& alternateRoutes)
{
    if (pin.portName.empty() || pin.pinName.empty())
        return ValidationResult::failure("board profile pin has an empty physical mapping");
    if (!virtualPins.insert(pin.virtualPin).second)
        return ValidationResult::failure("board profile contains a duplicate virtual pin");
    const auto physicalKey = pin.portName + "." + pin.pinName;
    if (!physicalPins.insert(physicalKey).second)
        return ValidationResult::failure("board profile contains a duplicate physical pin: " + physicalKey);
    if (pin.resetMode == PinResetMode::Input && !has_capability(pin.capabilities, PinCapability::DigitalInput))
        return ValidationResult::failure("board profile reset input is unsupported for pin " + std::to_string(pin.virtualPin));
    if (pin.resetMode == PinResetMode::Output && !has_capability(pin.capabilities, PinCapability::DigitalOutput))
        return ValidationResult::failure("board profile reset output is unsupported for pin " + std::to_string(pin.virtualPin));
    if (pin.resetPullMode == PinPullMode::PullUp && !has_capability(pin.capabilities, PinCapability::PullUp))
        return ValidationResult::failure("board profile reset pull-up is unsupported for pin " + std::to_string(pin.virtualPin));
    if (pin.resetPullMode == PinPullMode::PullDown && !has_capability(pin.capabilities, PinCapability::PullDown))
        return ValidationResult::failure("board profile reset pull-down is unsupported for pin " + std::to_string(pin.virtualPin));
    if (pin.outputType == PinOutputType::OpenDrain && !has_capability(pin.capabilities, PinCapability::OpenDrain))
        return ValidationResult::failure("board profile open-drain output is unsupported for pin " + std::to_string(pin.virtualPin));
    std::unordered_set<std::string> pinRoutes;
    for (const auto& alternate : pin.alternateFunctions) {
        if (alternate.name.empty() || alternate.route.empty())
            return ValidationResult::failure("board profile contains an empty alternate function");
        const auto key = alternate.name + "@" + alternate.route;
        if (!pinRoutes.insert(key).second || !alternateRoutes.insert(key).second)
            return ValidationResult::failure("board profile contains a conflicting alternate function: " + key);
    }
    return ValidationResult::success();
}

} // namespace

const BoardProfile* find_board_profile(const std::string& boardId)
{
    const auto& knownProfiles = profiles();
    const auto found = std::find_if(knownProfiles.begin(), knownProfiles.end(),
        [&](const BoardProfile& profile) { return profile.boardId == boardId; });
    return found == knownProfiles.end() ? nullptr : &*found;
}

ValidationResult validate_board_profile(const BoardProfile& profile)
{
    if (profile.boardId.empty())
        return ValidationResult::failure("board profile id is empty");
    std::unordered_set<std::uint8_t> virtualPins;
    std::unordered_set<std::string> physicalPins;
    std::unordered_set<std::string> alternateRoutes;
    for (const auto& pin : profile.pins) {
        const auto result = validate_pin_definition(pin, virtualPins, physicalPins, alternateRoutes);
        if (!result.valid)
            return result;
    }
    return ValidationResult::success();
}

ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const BoardProfile& profile)
{
    const auto profileResult = validate_board_profile(profile);
    if (!profileResult.valid)
        return profileResult;
    const auto baseResult = validate_firmware(package);
    if (!baseResult.valid)
        return baseResult;
    if (package.manifest.boardId != profile.boardId)
        return ValidationResult::failure("firmware board id does not match selected board profile");
    if (package.manifest.flashSize > profile.flashSize)
        return ValidationResult::failure("firmware flash requirement exceeds board profile capacity");
    if (package.manifest.ramSize > profile.ramSize)
        return ValidationResult::failure("firmware RAM requirement exceeds board profile capacity");
    for (const auto& peripheral : package.manifest.requiredPeripherals) {
        if (!supports(profile, peripheral))
            return ValidationResult::failure("firmware requires unsupported peripheral: " + peripheral);
    }
    return ValidationResult::success();
}

ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const std::string& boardId)
{
    const auto* profile = find_board_profile(boardId);
    if (profile == nullptr)
        return ValidationResult::failure("unknown board profile: " + boardId);
    return validate_firmware_for_board(package, *profile);
}

} // namespace rsma_mcu