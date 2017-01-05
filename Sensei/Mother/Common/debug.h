#define DEBUG (1)

#ifdef DEBUG
#define d(x) Serial.println(x)
#define dn(x) Serial.print(x)
#else
#define d(x)
#define dn(x)
#endif

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Werror"
