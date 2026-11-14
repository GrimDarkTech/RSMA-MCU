#include <rsma_mcu/Arduino.h>

bool packetDone = false;

void onPacket()
{
    if (Serial1.packetType() == 5) {
        if (Serial1.packetAvailable() != 0) {
            Serial1.write(Serial1.packetRead());
            packetDone = true;
        }
    }
}

int main()
{
    Serial1.begin(0);
    Serial1.attachPacketReceive(onPacket);
    while (!packetDone) {
        delay(1);
    }
    return 0;
}