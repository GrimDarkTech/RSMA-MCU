#include <rsma_mcu/Arduino.h>

int main()
{
    int spi = spiTransfer(1, 0xA5);
    int i2c = i2cTransfer(0x42, spi);
    return dmaTransfer(2, i2c);
}