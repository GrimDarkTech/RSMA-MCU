#include <rsma_mcu/Arduino.h>

int main()
{
    while (digitalRead(7) == 0) {
        delay(10);
    }
    return 0;
}