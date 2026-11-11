#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(7, INPUT);
    pinMode(13, OUTPUT);
    if (digitalRead(7)) {
        digitalWrite(13, HIGH);
    } else {
        digitalWrite(13, LOW);
    }
    return 0;
}