#include "accel.h"
#include <Arduino.h>

// Accelerometer variables
bool accelerometerOn;
int xAccelerometerPrev;
int zAccelerometerPrev;
int xAccelerometerDiff;
int zAccelerometerDiff;
int accelerometerCount;

////////// Accelerometer Functions //////////

void setupAccelerometer()
{
    pinMode(ACCEL_POWER_PIN, OUTPUT);
}

void enableAccelerometer()
{
    digitalWrite(ACCEL_POWER_PIN, HIGH);
}

void disableAccelerometer()
{
    digitalWrite(ACCEL_POWER_PIN, LOW);
}

void readAcc()
{
    int xAccelerometerCurr = analogRead(X_PIN);
    int zAccelerometerCurr = analogRead(Z_PIN);
    xAccelerometerDiff += (xAccelerometerCurr - xAccelerometerPrev) * (xAccelerometerCurr - xAccelerometerPrev);
    zAccelerometerDiff += (zAccelerometerCurr - zAccelerometerPrev) * (zAccelerometerCurr - zAccelerometerPrev);
    xAccelerometerPrev = xAccelerometerCurr;
    zAccelerometerPrev = zAccelerometerCurr;
    accelerometerCount++;
}

void clearAccelerometerState()
{
    xAccelerometerDiff = 0;
    zAccelerometerDiff = 0;
    xAccelerometerPrev = 0;
    zAccelerometerPrev = 0;
    accelerometerCount = 0;
}