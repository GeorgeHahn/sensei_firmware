/*
 * Time.h - Library for maintaining discrete date and time representation for PrNet nodes.
 * Created by Dwyane George, November 16, 2015
 * Social Computing Group, MIT Media Lab
 */

#ifndef PrTime_h
#define PrTime_h

#include "Arduino.h"

// Time structure
struct t {
  int month;
  int date;
  int year;
  int day;
  int hours;
  int minutes;
  int seconds;
};

class Time {
  public:
    struct t initialTime;
    struct t t;
    volatile unsigned long secondsElapsed;
    volatile unsigned long totalSecondsElapsed;
    bool isTimeSet;
    Time();
    void setInitialTime(int month, int date, int year, int day, int hours, int minutes, int seconds);
    void updateTime();
    bool timeout(unsigned long *counter, int milliseconds);
    bool inDataCollectionPeriod(int startHour, int startMinute, int endHour, int endMinute);
    void displayDateTime();
};

#endif
