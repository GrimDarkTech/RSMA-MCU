#include <rsma_mcu/Arduino.h>

void onPacket(int value)
{
}

int main()
{
    Serial.attachPacketReceive(onPacket);
}