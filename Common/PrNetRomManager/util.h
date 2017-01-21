#ifndef util_h
#define util_h

#include "Arduino.h"

void PrintHexInt(uint32_t data);
void PrintHexByte(uint8_t data);
uint8_t ReadHexByte();
char ReadChar();
void PrintBinaryByte(uint8_t data);

#endif
