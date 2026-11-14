#include <rsma_mcu/Arduino.h>

int main()
{
    Serial.begin(4);
    while (Serial.available() == 0) {
        delay(1);
    }
    int value = Serial.read();
    Serial.write(value);
    return 0;
}