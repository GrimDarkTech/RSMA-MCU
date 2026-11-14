#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(13, OUTPUT);
    int elapsed = millis();
    if (elapsed == 0) {
        digitalWrite(13, LOW);
    }
    delay(25);
    elapsed = millis();
    if (elapsed >= 25) {
        digitalWrite(13, HIGH);
    }
    return 0;
}