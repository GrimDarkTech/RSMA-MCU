#include <rsma_mcu/Arduino.h>

int main()
{
    Serial.begin(4);
    Serial.write(256);
}