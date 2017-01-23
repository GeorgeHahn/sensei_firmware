#ifndef util_h
#define util_h

#include "Arduino.h"

void PrintBinaryData(uint8_t *data, unsigned int len);
void PrintHexData(uint8_t *data, unsigned int len);
void PrintHexInt(uint32_t data);
void PrintHexByte(uint8_t data);
uint8_t ReadHexByte();
char ReadChar();
void PrintBinaryByte(uint8_t data);

#endif
