#include <rsma_mcu/Arduino.h>

// Register-level virtual sensor example. The Unity host maps an I2C device at
// 0x48 and an SPI device at bus 1/chip-select 3 before running this firmware.
int main()
{
    const int temperature = i2cReadRegister(0x48, 0x10);
    i2cWriteRegister(0x48, 0x11, temperature);
    const int status = spiReadRegister(1, 3, 0x12);
    spiWriteRegister(1, 3, 0x13, status);
    return 0;
}