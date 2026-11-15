#include <rsma_mcu/Arduino.h>

void onInputChange()
{
    digitalWrite(10, HIGH);
}

int main()
{
    pinMode(10, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(13, INPUT);
    attachInterrupt(13, onInputChange, CHANGE);
    Serial.begin(4);

    while (true) {
        analogWrite(6, analogRead(4) / 16, 20000);
        if (Serial.available() > 0)
            Serial.write(Serial.read());
        const int timestampUs = micros();
        if (timestampUs < 0)
            digitalWrite(10, LOW);
        delay(10);
    }
}