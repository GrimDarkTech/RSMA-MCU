#include <rsma_mcu/Arduino.h>

int read_level();

int main()
{
    digitalWrite(13, read_level());
    return 0;
}