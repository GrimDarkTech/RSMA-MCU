#include <rsma_mcu/Arduino.h>

bool callbackDone = false;

void onSerial1Receive()
{
    if (Serial1.available() != 0) {
        Serial1.write(Serial1.read());
        callbackDone = true;
    }
}

int main()
{
    Serial1.begin(0);
    Serial1.attachReceive(onSerial1Receive);
    while (!callbackDone) {
        delay(1);
    }
    return 0;
}