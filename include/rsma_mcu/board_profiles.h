#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rsma_mcu\firmware_format.h"
#include "rsma_mcu\firmware_validator.h"

namespace rsma_mcu {

struct BoardProfile
{
    std::string boardId;
    std::uint32_t flashSize = 0;
    std::uint32_t ramSize = 0;
    std::vector<std::string> supportedPeripherals;
};

const BoardProfile* find_board_profile(const std::string& boardId);
ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const BoardProfile& profile);
ValidationResult validate_firmware_for_board(const FirmwarePackage& package, const std::string& boardId);

} // namespace rsma_mcu