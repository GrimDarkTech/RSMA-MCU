#include "rsma_mcu\board_profiles.h"

#include <algorithm>

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
        },
        {
            "RSMA-AVR-ATmega328P",
            32 * 1024,
            2 * 1024,
            {"GPIO", "SYSTICK", "IRQ", "PWM", "UART", "ADC", "SPI", "I2C", "EEPROM"},
        },
    };
    return value;
}

bool supports(const BoardProfile& profile, const std::string& peripheral)
{
    return std::find(profile.supportedPeripherals.begin(), profile.supportedPeripherals.end(), peripheral) !=
        profile.supportedPeripherals.end();
}

} // namespace

const BoardProfile* find_board_profile(const std::string& boardId)
{
    const auto& knownProfiles = profiles();
    const auto found = std::find_if(knownProfiles.begin(), knownProfiles.end(),
        [&](const BoardProfile& profile) { return profile.boardId == boardId; });
    return found == knownProfiles.end() ? nullptr : &*found;
}

ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const BoardProfile& profile)
{
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