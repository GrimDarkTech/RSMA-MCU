#include <rsma_mcu/Arduino.h>

void onChange()
{
    digitalWrite(10, HIGH);
}

int main()
{
    pinMode(10, OUTPUT);
    pinMode(13, INPUT);
    pinMode(6, OUTPUT);
    attachInterrupt(13, onChange, CHANGE);
    analogWrite(6, 127, 20000);
    int now = micros();
    if (now >= 0)
        Serial.begin(4);
    if (Serial.available() > 0)
        Serial.write(Serial.read());
    detachInterrupt(13);
    return 0;
}