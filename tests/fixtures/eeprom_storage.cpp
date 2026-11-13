#include <rsma_mcu/Arduino.h>

int main()
{
    eepromWrite(7, 0x5A);
    return eepromRead(7);
}