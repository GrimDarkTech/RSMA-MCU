#include <rsma_mcu/Arduino.h>

int main()
{
    digitalWrite(13, HIGH);
    yield();
    digitalWrite(13, LOW);
    return 0;
}