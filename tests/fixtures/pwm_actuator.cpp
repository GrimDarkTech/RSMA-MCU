#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(10, OUTPUT);
    pinMode(13, OUTPUT);
    digitalWrite(10, HIGH);
    analogWrite(13, 192, 1000);
    delay(10);
    analogWrite(13, 0, 1000);
}