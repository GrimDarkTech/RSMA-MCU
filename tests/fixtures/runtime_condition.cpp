#include <rsma_mcu/Arduino.h>

int read_switch();

int main()
{
    if (read_switch()) {
        digitalWrite(13, HIGH);
    }
    return 0;
}