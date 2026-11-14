#include <rsma_mcu/Arduino.h>

int readButton() { return 1; }

int main()
{
    attachInterrupt(13, readButton, FALLING);
}