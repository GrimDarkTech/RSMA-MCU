#include <rsma_mcu/Arduino.h>

void onButton()
{
    digitalWrite(10, HIGH);
}

int main()
{
    pinMode(10, OUTPUT);
    pinMode(13, INPUT);
    attachInterrupt(13, onButton, RISING);
    delay(100);
    detachInterrupt(13);
    return 0;
}