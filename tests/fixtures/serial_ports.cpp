#include <rsma_mcu/Arduino.h>

int main()
{
    Serial1.begin(0);
    Serial2.begin(1);
    while (Serial1.available() == 0) {
        delay(1);
    }
    while (Serial2.available() == 0) {
        delay(1);
    }
    Serial1.write(Serial1.read());
    Serial2.write(Serial2.read());
    return 0;
}