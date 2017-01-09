#include <SimbleeCOM.h>
#include "rom.h"
#include "rtc.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// ROM Manager data structure
PrNetRomManager romManager;

// Data transfer protocol state variables
int transferPage;
int transferRow;

/*
 * Time format: (total seconds since 5am)/10 (fits into 13 bits)
 */
uint16_t GetTime()
{
    // Fetch latest time from the timer
    timer.updateTime();

    return (((((timer.t.hours - 5) * 60) + timer.t.minutes) * 60 + timer.t.seconds) / 10) && 0x1FFF;
}

/*
 * Takes an accel reading if appropriate, then dumps prox events to flash
 */
void writeData()
{
    // Check for room in flash
    if (romManager.OutOfSpace()) {
        return;
    }

#if REGION_TRACKER == false
    if (accelerometerCount > 0) {
        writeDataRow(ROW_ACCEL);
        clearAccelerometerState();
    }
#endif

    if (newData) {
        newData = false;
        writeDataRow(ROW_PROX);
    }
}

/*
 * Stores timestamp OR
 * Stores x and z accelerometer data OR
 * Stores proximity events with deviceID (I), RSSI (R), and time (T) OR
 * Stores reset marker as 999,999,999
 */
void writeDataRow(uint8_t data)
{
    // If this page is full, write it to flash
    romManager.CheckPageSpace();

    // Don't try to save if we're out of space
    if (romManager.OutOfSpace()) {
        return;
    }

    if (data == ROW_PROX) {
        // Proximity event
        for (int i = 0; i < NETWORK_SIZE; i++) {
            int rssiAverage =
                (rssiCount[i] == 0) ? -128 : rssiTotal[i] / rssiCount[i];
            Serial.println(String(i) + "\t" + String(i) + "\t" + String(i) + "\t" +
                           String(rssiAverage) + "\t" + String(rssiCount[i]));

            // If the average RSSI was strong enough, write a proximity event row
            if (rssiAverage > -100) {
                romManager.CheckPageSpace();
                if (romManager.OutOfSpace()) {
                    return;
                }

                // Convert rssiAverage to positive
                rssiAverage = -rssiAverage;

                // Write a row for this proximity event
                // 0b1, Time (13 bits), rssi (7 bits), unused (3 bits), ID (8 bits)
                // 0b1TTTTTTTTTTTTTRRRRRRRUUUIIIIIIII
                romManager.table.data[romManager.config.rowCounter] =
                    DATA_ROW_HEADER |          // 0b1...
                    GetTime() & 0x1FFF << 18 | // Time mask: 0x7FFC0000
                    rssiAverage & 0x7F << 11 | // RSSI mask: 0x0003F800
                    // Bits 8, 9, 10 are free
                    i & 0xFF; // Device ID mask: 0x000000FF
                romManager.config.rowCounter++;
            }

            // Reset average for this device
            rssiTotal[i] = 0;
            rssiCount[i] = 0;
        }

        return;
    }

    if (data == ROW_TIME) {
        timer.updateTime();
        romManager.table.data[romManager.config.rowCounter] =
            TIME_ROW_HEADER | GetTime();
    } else if (data == ROW_ACCEL) {
        // 30 bits
        // ZZZZZZZZZZZZZZZXXXXXXXXXXXXXXX
        romManager.table.data[romManager.config.rowCounter] =
            ACCEL_ROW_HEADER |
            (min(xAccelerometerDiff / (10 * accelerometerCount), 0x7FFF)) |
            (min(zAccelerometerDiff / (10 * accelerometerCount), 0x7FFF) << 15);
    } else if (data == ROW_RESET) {
        romManager.table.data[romManager.config.rowCounter] = RESET_ROW_HEADER;
    }
    romManager.config.rowCounter++;
}

/*
 * Send response for ROM data to mother node
 */
void sendROMResponse()
{
    // TODO REPLACE WITH OTHER VERSION
}

/*
 * Remotely erases sensors after successful data transfer to mother node
 */
void remoteEraseROM()
{
    Serial.println("Erasing ROM");
    romManager.eraseROM();
}

/*
 * Erases ROM and resets ROM configuration if sensor is reprogrammed or write
 * ROM reset otherwise
 */
void resetROM()
{
    (romManager.config.pageCounter == -1) ? romManager.resetConfig()
                                          : writeDataRow(ROW_RESET);
}
