#include "Arduino.h"
#include "HardwareSerial.h"

void PrintHexInt(uint32_t data)
{
	char tmp[9];
	for (int8_t i=7; i >= 0; i--)
	{
		uint8_t temp = data & 0x0F;
		if(temp < 0x0A)
			tmp[i] = '0' + temp;
		else
			tmp[i] = 'A' - 0xA + temp;
		data >>= 4;
	}
	tmp[8] = 0;
	Serial.print(tmp);
}

void PrintHexByte(uint8_t data)
{
	char tmp[3];
	for (int8_t i=1; i >= 0; i--)
	{
		uint8_t temp = data & 0x0F;
		if(temp < 0x0A)
			tmp[i] = '0' + temp;
		else
			tmp[i] = 'A' - 0xA + temp;
		data >>= 4;
	}
	tmp[2] = 0;
	Serial.print(tmp);
}
