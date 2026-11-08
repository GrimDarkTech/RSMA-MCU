#pragma once

#include <string>

#include "rsma_mcu/firmware_format.h"

namespace rsma_mcu {

struct ValidationResult
{
    bool valid = false;
    std::string error;

    static ValidationResult success() { return {true, {}}; }
    static ValidationResult failure(std::string message) { return {false, std::move(message)}; }
};

ValidationResult validate_firmware(const FirmwarePackage& package);
ValidationResult validate_firmware_bytes(const Bytes& bytes);

} // namespace rsma_mcu
