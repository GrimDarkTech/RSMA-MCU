#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(13, OUTPUT);
    for (int i = 0; i < 3; ++i) {
        digitalWrite(13, HIGH);
        delay(10);
    }
    return 0;
}