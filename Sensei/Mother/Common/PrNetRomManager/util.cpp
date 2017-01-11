#include "Arduino.h"
#include "HardwareSerial.h"
#include "..\debug.h"

void PrintHexInt(uint32_t data)
{
    char tmp[9];
    for (int8_t i = 7; i >= 0; i--) {
        uint8_t temp = data & 0x0F;
        if (temp < 0x0A)
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
    for (int8_t i = 1; i >= 0; i--) {
        uint8_t temp = data & 0x0F;
        if (temp < 0x0A)
            tmp[i] = '0' + temp;
        else
            tmp[i] = 'A' - 0xA + temp;
        data >>= 4;
    }
    tmp[2] = 0;
    Serial.print(tmp);
}

char ReadChar()
{
    while (Serial.available() == 0) {
        delay(1);
    }
    char ret = Serial.read();
    dn(ret);
    return ret;
}

uint8_t ReadHexNibble()
{
    char in = ReadChar();
    if (in >= '0' && in <= '9') {
        return in - '0';
    } else if (in >= 'A' && in <= 'F') {
        return in - 'A' + 0xA;
    } else if (in >= 'a' && in <= 'f') {
        return in - 'a' + 0xA;
    }

    // INVALID CHARACTER
    return 0xFF;
}

uint8_t ReadHexByte()
{
    uint8_t ret = ReadHexNibble() << 4 | ReadHexNibble();
    return ret;
}
