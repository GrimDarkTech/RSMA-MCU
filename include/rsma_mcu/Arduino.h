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
void dmaBufferWrite(int buffer, int offset, int value);
int dmaBufferRead(int buffer, int offset);
int dmaSubmit(int channel, int buffer, int length);
int dmaStatus(int ticket);
int dmaReadByte(int ticket, int offset);
int dmaRelease(int ticket);
int eepromRead(int address);
void eepromWrite(int address, int value);
void delay(unsigned int milliseconds);
void yield();

#define DMA_INVALID (-1)
#define DMA_QUEUE_FULL (-2)
#define DMA_QUEUED 0
#define DMA_ACTIVE 1
#define DMA_COMPLETE 2

}