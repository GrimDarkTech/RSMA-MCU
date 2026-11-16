#include <rsma_mcu/Arduino.h>

int main()
{
    pinMode(13, OUTPUT);
    Serial.begin(4);
    while (true) {
        int rawTemperature = i2cReadRegister(0x48, 0x10);
        int filteredTemperature = rawTemperature - 32;
        if (filteredTemperature >= 200)
            analogWrite(13, 192);
        else
            analogWrite(13, 0);
        Serial.write(filteredTemperature);
        delay(10);
    }
}