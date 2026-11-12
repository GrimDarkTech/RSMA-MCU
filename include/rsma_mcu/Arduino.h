#pragma once

#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1

extern "C" {

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
void delay(unsigned int milliseconds);
void yield();

}