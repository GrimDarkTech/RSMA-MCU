#include "rsma_mcu\board_profiles.h"

#include <algorithm>
#include <unordered_set>

namespace rsma_mcu {
namespace {

const std::vector<BoardProfile>& profiles()
{
    const auto make_stm32_pins = [] {
        std::vector<PinDefinition> pins;
        for (std::uint8_t pinNumber = 0; pinNumber < 16; ++pinNumber) {
            PinDefinition pin;
            pin.virtualPin = pinNumber;
            pin.portName = "PC";
            pin.pinName = std::to_string(pinNumber);
            pin.capabilities = PinCapability::DigitalInput | PinCapability::DigitalOutput |
                PinCapability::PullUp | PinCapability::PullDown | PinCapability::OpenDrain |
                PinCapability::ExternalInterrupt;
            pin.alternateFunctions.push_back({"EXTI", "EXTI" + std::to_string(pinNumber)});
            if (pinNumber <= 5 || pinNumber == 10) {
                pin.capabilities = pin.capabilities | PinCapability::Analog;
                pin.alternateFunctions.push_back({"ADC", "ADC" + std::to_string(pinNumber)});
            }
            if (pinNumber == 6 || pinNumber == 7 || pinNumber == 13) {
                pin.capabilities = pin.capabilities | PinCapability::Pwm;
                pin.alternateFunctions.push_back({"PWM", pinNumber == 6 ? "TIM3_CH1" :
                    pinNumber == 7 ? "TIM3_CH2" : "TIM1_CH1"});
            }
            if (pinNumber == 8 || pinNumber == 10 || pinNumber == 13)
                pin.alternateFunctions.push_back({"SPI", pinNumber == 8 ? "SPI1_MISO" :
                    pinNumber == 10 ? "SPI1_MOSI" : "SPI1_SCK"});
            if (pinNumber == 9 || pinNumber == 11)
                pin.alternateFunctions.push_back({"I2C", pinNumber == 9 ? "I2C1_SCL" : "I2C1_SDA"});
            if (pinNumber == 10)
                pin.physicalRoutes.push_back({"PB", "13"});
            if (pinNumber == 11)
                pin.physicalRoutes.push_back({"PB", "14"});
            if (pinNumber == 12)
                pin.physicalRoutes.push_back({"PA", "10"});
            if (pinNumber == 13)
                pin.physicalRoutes.push_back({"PA", "11"});
            pins.push_back(std::move(pin));
        }
        return pins;
    };
    const auto make_avr_pins = [] {
        std::vector<PinDefinition> pins;
        for (std::uint8_t pinNumber = 8; pinNumber < 14; ++pinNumber) {
            PinDefinition pin;
            pin.virtualPin = pinNumber;
            pin.portName = "PB";
            pin.pinName = std::to_string(pinNumber - 8);
            pin.capabilities = PinCapability::DigitalInput | PinCapability::DigitalOutput |
                PinCapability::OpenDrain | PinCapability::ExternalInterrupt;
            pin.alternateFunctions.push_back({"EXTI", "EXTI" + std::to_string(pinNumber - 8)});
            if (pinNumber == 8 || pinNumber == 9 || pinNumber == 11) {
                pin.capabilities = pin.capabilities | PinCapability::Pwm;
                pin.alternateFunctions.push_back({"PWM", pinNumber == 8 ? "TIM0_CH0" :
                    pinNumber == 9 ? "TIM0_CH1" : "TIM2_CH2"});
            }
            if (pinNumber == 10 || pinNumber == 11)
                pin.alternateFunctions.push_back({"SPI", pinNumber == 10 ? "SPI0_SS" : "SPI0_MOSI"});
            if (pinNumber == 12)
                pin.alternateFunctions.push_back({"I2C", "I2C0_SDA"});
            if (pinNumber == 13) {
                pin.capabilities = pin.capabilities | PinCapability::Analog;
                pin.alternateFunctions.push_back({"ADC", "ADC0"});
            }
            pins.push_back(std::move(pin));
        }
        return pins;
    };
    static const std::vector<BoardProfile> value{
        {
            "RSMA-STM32F103",
            64 * 1024,
            20 * 1024,
            {"GPIO", "SYSTICK", "IRQ", "PWM", "UART", "ADC", "SPI", "I2C", "DMA", "EEPROM"},
            make_stm32_pins(),
        },
        {
            "RSMA-AVR-ATmega328P",
            32 * 1024,
            2 * 1024,
            {"GPIO", "SYSTICK", "IRQ", "PWM", "UART", "ADC", "SPI", "I2C", "EEPROM"},
            make_avr_pins(),
        },
    };
    return value;
}

bool supports(const BoardProfile& profile, const std::string& peripheral)
{
    return std::find(profile.supportedPeripherals.begin(), profile.supportedPeripherals.end(), peripheral) !=
        profile.supportedPeripherals.end();
}

bool supports_route(const BoardProfile& profile, const std::string& requirement)
{
    const auto separator = requirement.find('@');
    const auto name = separator == std::string::npos ? std::string{} : requirement.substr(0, separator);
    const auto route = separator == std::string::npos ? requirement : requirement.substr(separator + 1);
    for (const auto& pin : profile.pins) {
        for (const auto& alternate : pin.alternateFunctions) {
            if ((name.empty() || alternate.name == name) && alternate.route == route)
                return true;
        }
    }
    return false;
}

const PinDefinition* find_pin(const BoardProfile& profile, std::uint8_t virtualPin)
{
    const auto found = std::find_if(profile.pins.begin(), profile.pins.end(),
        [&](const PinDefinition& pin) { return pin.virtualPin == virtualPin; });
    return found == profile.pins.end() ? nullptr : &*found;
}

bool has_alternate(const PinDefinition& pin, const std::string& name)
{
    return std::any_of(pin.alternateFunctions.begin(), pin.alternateFunctions.end(),
        [&](const AlternateFunction& alternate) { return alternate.name == name; });
}

ValidationResult validate_firmware_pin_usage(const FirmwarePackage& package, const BoardProfile& profile)
{
    for (const auto& instruction : package.bytecode.instructions) {
        if (instruction.opcode == Opcode::AttachIrq) {
            const auto* pin = find_pin(profile, instruction.operands[0]);
            if (pin == nullptr)
                return ValidationResult::failure("firmware IRQ references an unmapped virtual pin");
            if (!has_capability(pin->capabilities, PinCapability::ExternalInterrupt) ||
                !has_alternate(*pin, "EXTI"))
                return ValidationResult::failure("firmware IRQ references a pin without EXTI capability");
            continue;
        }
        if (instruction.opcode != Opcode::HalCall)
            continue;
        const auto service = static_cast<HalService>(
            static_cast<std::uint16_t>(instruction.operands[0]) |
            (static_cast<std::uint16_t>(instruction.operands[1]) << 8));
        const auto virtualPin = instruction.operands[2];
        const auto* pin = find_pin(profile, virtualPin);
        if (pin == nullptr &&
            (service == HalService::PinMode || service == HalService::DigitalWrite ||
                service == HalService::DigitalRead || service == HalService::ConfigurePin ||
                service == HalService::PwmWrite || service == HalService::PwmWriteFrequency ||
                service == HalService::AnalogWrite ||
                service == HalService::AnalogRead))
            return ValidationResult::failure("firmware HAL call references an unmapped virtual pin");
        if (pin == nullptr)
            continue;
        switch (service) {
        case HalService::PinMode:
            if (instruction.operands[3] == 0 && !has_capability(pin->capabilities, PinCapability::DigitalInput))
                return ValidationResult::failure("firmware pin mode requires digital input capability");
            if (instruction.operands[3] == 1 && !has_capability(pin->capabilities, PinCapability::DigitalOutput))
                return ValidationResult::failure("firmware pin mode requires digital output capability");
            break;
        case HalService::DigitalRead:
            if (!has_capability(pin->capabilities, PinCapability::DigitalInput))
                return ValidationResult::failure("firmware digital read requires digital input capability");
            break;
        case HalService::DigitalWrite:
            if (!has_capability(pin->capabilities, PinCapability::DigitalOutput))
                return ValidationResult::failure("firmware digital write requires digital output capability");
            break;
        case HalService::ConfigurePin: {
            const auto config = instruction.operands[3];
            const auto mode = config & 0x01;
            if (mode == 0 && !has_capability(pin->capabilities, PinCapability::DigitalInput))
                return ValidationResult::failure("firmware GPIO configuration requires digital input capability");
            if (mode == 1 && !has_capability(pin->capabilities, PinCapability::DigitalOutput))
                return ValidationResult::failure("firmware GPIO configuration requires digital output capability");
            const auto pull = (config >> 1) & 0x03;
            if (pull == static_cast<std::uint8_t>(PinPullMode::PullUp) &&
                !has_capability(pin->capabilities, PinCapability::PullUp))
                return ValidationResult::failure("firmware GPIO configuration requires pull-up capability");
            if (pull == static_cast<std::uint8_t>(PinPullMode::PullDown) &&
                !has_capability(pin->capabilities, PinCapability::PullDown))
                return ValidationResult::failure("firmware GPIO configuration requires pull-down capability");
            if ((config & 0x08) != 0 && !has_capability(pin->capabilities, PinCapability::OpenDrain))
                return ValidationResult::failure("firmware GPIO configuration requires open-drain capability");
            break;
        }
        case HalService::PwmWrite:
        case HalService::PwmWriteFrequency:
        case HalService::AnalogWrite:
            if (!has_capability(pin->capabilities, PinCapability::Pwm) || !has_alternate(*pin, "PWM"))
                return ValidationResult::failure("firmware PWM call references a pin without PWM route");
            break;
        case HalService::AnalogRead:
            if (!has_capability(pin->capabilities, PinCapability::Analog) || !has_alternate(*pin, "ADC"))
                return ValidationResult::failure("firmware ADC call references a pin without ADC route");
            break;
        case HalService::SpiReadRegister:
        case HalService::SpiWriteRegister:
        case HalService::I2cReadRegister:
        case HalService::I2cWriteRegister:
            // Register-level bus routes are validated by the bus/device mapping;
            // these HAL calls do not consume a GPIO pin operand.
            break;
        default:
            break;
        }
    }
    return ValidationResult::success();
}

ValidationResult validate_pin_definition(const PinDefinition& pin,
    std::unordered_set<std::uint8_t>& virtualPins,
    std::unordered_set<std::string>& physicalPins,
    std::unordered_set<std::string>& alternateRoutes,
    std::unordered_set<std::string>& declaredPhysicalRoutes)
{
    if (pin.portName.empty() || pin.pinName.empty())
        return ValidationResult::failure("board profile pin has an empty physical mapping");
    if (!virtualPins.insert(pin.virtualPin).second)
        return ValidationResult::failure("board profile contains a duplicate virtual pin");
    const auto physicalKey = pin.portName + "." + pin.pinName;
    if (!physicalPins.insert(physicalKey).second)
        return ValidationResult::failure("board profile contains a duplicate physical pin: " + physicalKey);
    for (const auto& physicalRoute : pin.physicalRoutes) {
        if (physicalRoute.portName.empty() || physicalRoute.pinName.empty())
            return ValidationResult::failure("board profile contains an empty alternate physical route");
        const auto routeKey = physicalRoute.portName + "." + physicalRoute.pinName;
        if (!declaredPhysicalRoutes.insert(routeKey).second)
            return ValidationResult::failure("board profile contains a duplicate physical route: " + routeKey);
    }
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
    std::unordered_set<std::string> declaredPhysicalRoutes;
    for (const auto& pin : profile.pins) {
        const auto result = validate_pin_definition(pin, virtualPins, physicalPins, alternateRoutes,
            declaredPhysicalRoutes);
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
    for (const auto& route : package.manifest.requiredRoutes) {
        if (!supports_route(profile, route))
            return ValidationResult::failure("firmware requires unavailable board route: " + route);
    }
    const auto usageResult = validate_firmware_pin_usage(package, profile);
    if (!usageResult.valid)
        return usageResult;
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