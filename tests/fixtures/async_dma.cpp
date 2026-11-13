#include <rsma_mcu/Arduino.h>

int main()
{
    dmaBufferWrite(0, 0, 0xA5);
    int staged = dmaBufferRead(0, 0);
    dmaBufferWrite(0, 1, staged);
    int ticket = dmaSubmit(1, 0, 2);
    int status = dmaStatus(ticket);
    int received = dmaReadByte(ticket, 0);
    dmaBufferWrite(0, 2, received + status - status);
    return dmaRelease(ticket);
}