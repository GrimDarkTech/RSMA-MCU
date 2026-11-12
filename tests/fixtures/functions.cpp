#include <rsma_mcu/Arduino.h>

int get_level(int level)
{
    int result = level + 1;
    return result;
}

void set_led()
{
    digitalWrite(13, HIGH);
}

int main()
{
    pinMode(13, OUTPUT);
    if (get_level(0)) {
        set_led();
    }
    return 0;
}