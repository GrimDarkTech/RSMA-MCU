#include <rsma_mcu/Arduino.h>

int get_level()
{
    return 1;
}

void set_led()
{
    digitalWrite(13, HIGH);
}

int main()
{
    pinMode(13, OUTPUT);
    if (get_level()) {
        set_led();
    }
    return 0;
}