#include <rsma_mcu/Arduino.h>

// End-to-end source slice:
// I2C temperature register -> integer calibration/filter -> fan PWM -> UART telemetry.
// The virtual host maps the sensor at 0x48/register 0x10 and uses PWM pin 13.
int main()
{
    pinMode(13, OUTPUT);
    Serial.begin(4); // 115200 baud preset.

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