#include <rsma_mcu/Arduino.h>

int main()
{
    i2cReadRegister(0x48, 0x10);
    i2cWriteRegister(0x48, 0x10, 0x2A);
    spiReadRegister(1, 3, 0x12);
    spiWriteRegister(1, 3, 0x12, 0xA5);
    return 0;
}