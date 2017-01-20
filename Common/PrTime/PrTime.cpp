/*
 * Time.cpp - Library for maintaining discrete date and time representation for PrNet nodes.
 * Created by Dwyane George, November 16, 2015
 * Social Computing Group, MIT Media Lab
 */

#include "Arduino.h"
#include "PrTime.h"

Time::Time() {}

/*
 * Sets the initial time
 */
void Time::setInitialTime(int month, int date, int year, int day, int hours, int minutes, int seconds)
{
    initialTime.month = month;
    initialTime.date = date;
    initialTime.year = year;
    initialTime.day = day;
    initialTime.hours = hours;
    initialTime.minutes = minutes;
    initialTime.seconds = seconds;
    t.month = month;
    t.date = date;
    t.year = year;
    t.day = day;
    t.hours = hours;
    t.minutes = minutes;
    t.seconds = seconds;
    secondsElapsed = 0;
    isTimeSet = true;
}

void Time::NextSecond()
{
    if (!isTimeSet) {
        return;
    }

    t.seconds++; // we don't particularly care if this goes over 60
    secondsElapsed++;
    totalSecondsElapsed++;
}

/*
 * Updates the discrete representation of the time
 * GH 1/10/17: This is disgusting, delete ASAP
 */
void Time::updateTime()
{
    if (!isTimeSet) {
        return;
    }

    int carryOver;
    // Calculate each time component and propagate carryOver linearly
    t.seconds = (initialTime.seconds + secondsElapsed) % 60;
    carryOver = (initialTime.seconds + secondsElapsed) / 60;
    if (carryOver == 0) {
        return;
    }

    t.minutes = (initialTime.minutes + carryOver) % 60;
    carryOver = (initialTime.minutes + carryOver) / 60;
    if (carryOver == 0) {
        return;
    }

    t.hours = (initialTime.hours + carryOver) % 24;
    carryOver = (initialTime.hours + carryOver) / 24;
    if (carryOver == 0) {
        return;
    }

    t.day = (initialTime.day + carryOver) % 7;
    // If in February, check if leap year
    if (t.month == 2) {
        if (t.year % 4 != 0) {
            t.date = (initialTime.date - 1 + carryOver) % 28 + 1;
            carryOver = (initialTime.date - 1 + carryOver) / 28;
        } else {
            t.date = (initialTime.date - 1 + carryOver) % 29 + 1;
            carryOver = (initialTime.date - 1 + carryOver) / 29;
        }
        // If in April, June, September, or November
    } else if (t.month == 4 || t.month == 6 || t.month == 9 || t.month == 11) {
        t.date = (initialTime.date - 1 + carryOver) % 30 + 1;
        carryOver = (initialTime.date - 1 + carryOver) / 30;
    } else {
        t.date = (initialTime.date - 1 + carryOver) % 31 + 1;
        carryOver = (initialTime.date - 1 + carryOver) / 31;
    }
    t.month = (initialTime.month - 1 + carryOver) % 12 + 1;
    carryOver = (initialTime.month - 1 + carryOver) / 12;
    t.year = (initialTime.year + carryOver) % 99;
}

/*
 * Checks if given milliseconds have elapsed
 */
bool Time::timeout(unsigned long *counter, int milliseconds)
{
    return (millis() - *counter >= milliseconds);
}

/*
 * Checks if current time is within data collection period
 */
bool Time::inDataCollectionPeriod(int startHour, int startMinute, int endHour, int endMinute)
{
    updateTime();
    if (!isTimeSet) {
        return false;
    }

    // Check the weekday (No Saturdays/Sundays)
    if (t.day == 0 || t.day == 6) {
        return false;
    }

    // Collect data while within range
    if ((t.hours > startHour) && (t.hours < endHour)) {
        return true;
    }

    // If start and end hour if the same, check minute on both endpoints
    if ((startHour == endHour) && ((t.minutes < startMinute) || (t.minutes >= endMinute))) {
        return false;
    }

    // Inclusive on startMinute and exclusive on endMinute
    if (((t.hours == startHour) && (t.minutes >= startMinute)) || ((t.hours == endHour) && (t.minutes < endMinute))) {
        return true;
    }

    // Otherwise, not in data collection period
    return false;
}

/*
 * Prints date and time representation of the current time
 */
void Time::displayDateTime()
{
    if (isTimeSet) {
        Serial.print(t.month);
        Serial.print("/");
        Serial.print(t.date);
        Serial.print("/");
        Serial.print(t.year);
        Serial.print(" ");
        Serial.print(t.day);
        Serial.print(" ");
        Serial.print(t.hours);
        Serial.print(":");
        Serial.print(t.minutes);
        Serial.print(":");
        Serial.println(t.seconds);
    } else {
        Serial.println("Time not set");
    }
}
