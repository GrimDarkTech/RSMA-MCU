#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(13, OUTPUT);
    {
        digitalWrite(13, HIGH);
        delay(500);
    }
    digitalWrite(13, LOW);
    return 0;
}