#include "ds3231/ds3231.h"
#include "PrTime/PrTime.h"

#define RTC_INTERRUPT_PIN 10

#define DS3231_ADDR 0x68
#define CTRL_REG 0x0E
#define START_INT 0b01000000
#define STOP_INT 0b00000100

extern Time timer;
extern struct ts rtcTime;

// Time keeping and time sharing variables
extern int interruptTime;
extern unsigned long ms;

void setRTCTime(int month, int date, int year, int day, int hours, int minutes, int seconds);
void EnableRTCInterrupt();
void DisableRTCInterrupt();

void programSystemTime();
