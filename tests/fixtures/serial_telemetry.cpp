#include <rsma_mcu/Arduino.h>

int main()
{
    Serial.begin(4);
    Serial.write(0x54);
    Serial.write(0x45);
    Serial.write(0x4C);
    Serial.write(0x0A);
    while (Serial.available() == 0) {
        delay(1);
    }
    Serial.write(Serial.read());
}