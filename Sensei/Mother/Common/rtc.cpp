#include <SimbleeCOM.h>
#include "Wire\Wire.h"
#include "config.h"
#include "rtc.h"
#include "rom.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// Time keeping data structure
Time timer;
// RTC time data structure
struct ts rtcTime;

// Time keeping and time sharing variables
int interruptTime;
unsigned long ms;
bool foundRTCTransition;

/*
 * Starts the RTC clock
 */
void setRTCTime(int month, int date, int year, int day, int hours, int minutes, int seconds)
{
    rtcTime.mon = month;
    rtcTime.mday = date;
    rtcTime.year = 2000 + year;
    rtcTime.wday = day;
    rtcTime.hour = hours;
    rtcTime.min = minutes;
    rtcTime.sec = seconds;
    DS3231_set(rtcTime);
}

/*
 * Starts the RTC interrupt
 */
void EnableRTCInterrupt()
{
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(CTRL_REG);
    Wire.write(START_INT);
    Wire.endTransmission();
}

/*
 * Stops the RTC interrupt
 */
void DisableRTCInterrupt()
{
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(CTRL_REG);
    Wire.write(STOP_INT);
    Wire.endTransmission();
}
////////// Time Functions //////////

/*
 * Sets system time sent from serial monitor
 * Format: 
 */
void programSystemTime()
{
    d("Waiting for time");

    char systemTime[16];
    while (Serial.available() < 16) {
        delay(10);
    }
    Serial.readBytes(systemTime, 16);
    for (int i = 0; i < 16; i++) {
        Serial.print(systemTime[i]);
    }
    Serial.println();
    int initialMS = (systemTime[13] - '0') * 100 + (systemTime[14] - '0') * 10 + (systemTime[15] - '0');
    delay(1000 - initialMS);
    uint8_t month = (systemTime[0] - '0') * 10 + (systemTime[1] - '0');    // MM
    uint8_t date = (systemTime[2] - '0') * 10 + (systemTime[3] - '0');     // DD
    uint8_t year = (systemTime[4] - '0') * 10 + (systemTime[5] - '0');     // YY
    uint8_t day = (systemTime[6] - '0');                                   // Day
    uint8_t hour = (systemTime[7] - '0') * 10 + (systemTime[8] - '0');     // HH
    uint8_t minute = (systemTime[9] - '0') * 10 + (systemTime[10] - '0');  // MM
    uint8_t second = (systemTime[11] - '0') * 10 + (systemTime[12] - '0'); // SS
    timer.totalSecondsElapsed = 1;
    timer.secondsElapsed = 1;
    d(String(month));
    d(String(date));
    d(String(year));
    d(String(day));
    d(String(hour));
    d(String(minute));
    d(String(second));
    setRTCTime(month, date, year, day, hour, minute, second);
    timer.setInitialTime(month, date, year, day, hour, minute, second);
    Serial.print("Time set to ");
    timer.displayDateTime();
    EnableRTCInterrupt();
}
