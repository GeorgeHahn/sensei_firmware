#define DEBUG (1)

#ifdef DEBUG
#define d(x) Serial.println(x)
#define dn(x) Serial.print(x)
#define PrintByteDebug(x) PrintHexByte(x)
#define PrintByte(x) PrintHexByte(x)
#define PrintInt(x) PrintHexInt(x)
#else
#define d(x)
#define dn(x)
#define PrintByteDebug(x)
#define PrintByte(x) PrintBinaryByte(x)
#define PrintInt(x) PrintBinaryInt(x)
#endif

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Werror"
