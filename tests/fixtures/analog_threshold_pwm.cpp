#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(10, INPUT);
    pinMode(13, OUTPUT);
    while (true) {
        int sample = analogRead(10);
        if (sample >= 2048) {
            analogWrite(13, 255);
        } else {
            analogWrite(13, 0);
        }
        delay(10);
    }
}