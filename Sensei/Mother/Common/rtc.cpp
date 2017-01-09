#include <SimbleeCOM.h>
#include <Wire.h>
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
void setRTCTime()
{
    timer.updateTime();
    rtcTime.mon = timer.t.month;
    rtcTime.mday = timer.t.date;
    rtcTime.year = 2000 + timer.t.year;
    rtcTime.wday = timer.t.day;
    rtcTime.hour = timer.t.hours;
    rtcTime.min = timer.t.minutes;
    rtcTime.sec = timer.t.seconds;
    DS3231_set(rtcTime);
}

/*
 * Starts the RTC interrupt
 */
void startInterrupt()
{
    Wire.beginOnPins(14, 13);
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(CTRL_REG);
    Wire.write(START_INT);
    Wire.endTransmission();
}

/*
 * Stops the RTC interrupt
 */
void stopInterrupt()
{
    Wire.beginOnPins(14, 13);
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
        Serial.print(systemTime[i], HEX);
    }
    Serial.println();
    int initialMS = (systemTime[13] - '0') * 100 + (systemTime[14] - '0') * 10 + (systemTime[15] - '0');
    delay(1000 - initialMS);
    timer.setInitialTime((systemTime[0] - '0') * 10 + (systemTime[1] - '0'),    // MM
                         (systemTime[2] - '0') * 10 + (systemTime[3] - '0'),    // DD
                         (systemTime[4] - '0') * 10 + (systemTime[5] - '0'),    // YY
                         (systemTime[6] - '0'),                                 // Day
                         (systemTime[7] - '0') * 10 + (systemTime[8] - '0'),    // HH
                         (systemTime[9] - '0') * 10 + (systemTime[10] - '0'),   // MM
                         (systemTime[11] - '0') * 10 + (systemTime[12] - '0')); // SS
    timer.totalSecondsElapsed = 1;
    timer.secondsElapsed = 1;
    setRTCTime();
    startInterrupt();
    Serial.print("Time set to ");
    timer.displayDateTime();
}
