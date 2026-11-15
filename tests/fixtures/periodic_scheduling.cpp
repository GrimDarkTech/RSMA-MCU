#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(13, OUTPUT);
    int last = millis();
    while (millis() - last < 10) {
        yield();
    }
    digitalWrite(13, HIGH);
    delay(10);
    digitalWrite(13, LOW);
}