#include <rsma_mcu/Arduino.h>

void onButton() { }

int main()
{
    attachInterrupt(13, onButton, 7);
}