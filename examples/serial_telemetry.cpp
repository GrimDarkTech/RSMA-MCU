#include <rsma_mcu/Arduino.h>

// Minimal source-level telemetry/command endpoint example. The Unity scene can
// observe the four telemetry bytes and inject a command byte through UART.
int main()
{
    Serial.begin(4); // 115200 baud preset.
    Serial.write('T');
    Serial.write('E');
    Serial.write('L');
    Serial.write('\n');

    while (Serial.available() == 0) {
        delay(1);
    }
    Serial.write(Serial.read());
}