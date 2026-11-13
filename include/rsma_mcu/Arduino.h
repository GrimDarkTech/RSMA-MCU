#pragma once

#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1

extern "C" {

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int analogRead(int channel);
int spiTransfer(int bus, int value);
int i2cTransfer(int address, int value);
int dmaTransfer(int channel, int value);
int eepromRead(int address);
void eepromWrite(int address, int value);
void delay(unsigned int milliseconds);
void yield();

}