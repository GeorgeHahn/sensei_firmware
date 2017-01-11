#include <SimbleeCOM.h>
#include "rom.h"
#include "rtc.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// ROM Manager data structure
PrNetRomManager romManager;

/*
 * Time format: (total seconds since 5am)/10 (fits into 13 bits)
 */
uint16_t GetTime()
{
    // Update timer numbers
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
 * Send request for ROM data to network nodes
 */
void RequestROMFull(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_FULL, id};
    SimbleeCOM.send(payload, sizeof(payload));
    Serial.println("D " + String(id));
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
            int rssiAverage = (rssiCount[i] == 0) ? -128 : rssiTotal[i] / rssiCount[i];
            Serial.println(String(i) + "\t" + String(i) + "\t" + String(i) + "\t" + String(rssiAverage) + "\t" + String(rssiCount[i]));

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
                romManager.table.data[romManager.config.rowCounter] = DATA_ROW_HEADER |          // 0b1...
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
        romManager.table.data[romManager.config.rowCounter] = TIME_ROW_HEADER | GetTime();
    } else if (data == ROW_ACCEL) {
        // 30 bits
        // ZZZZZZZZZZZZZZZXXXXXXXXXXXXXXX
        romManager.table.data[romManager.config.rowCounter] = ACCEL_ROW_HEADER |
                                                              (min(xAccelerometerDiff / (10 * accelerometerCount), 0x7FFF)) |
                                                              (min(zAccelerometerDiff / (10 * accelerometerCount), 0x7FFF) << 15);
    } else if (data == ROW_RESET) {
        romManager.table.data[romManager.config.rowCounter] = RESET_ROW_HEADER;
    }
    romManager.config.rowCounter++;
}

/*
 * Send response for ROM data to mother node
	//  command
	//  Page
	//  Row
	//  Data[]
 */
void sendROMPage(uint8_t pageNumber)
{
    d("Transferring Page " + String(pageNumber));
    data *p = (data *)ADDRESS_OF_PAGE(pageNumber);
    int row = 0;
    char payload[15];
    bool success = false;

    // Don't listen for packets while we're sending ROM
    SimbleeCOM.stopReceive();
    while (row < MAX_ROWS) {
        payload[0] = RADIO_RESPONSE_ROWS; // 0; // TODO: This byte can be swapped out with some other data (but what?)
        payload[1] = pageNumber;          // TODO Can these two be replaced by an incrementing counter?
        payload[2] = row;
        for (int i = 0; i < 4; i++) {
            // Read next 3 ints and incrementally shift bytes into place
            payload[3 + i] = (p->data[row] >> (8 * i)) & 0xFF;
            if (row + 1 < MAX_ROWS) {
                payload[7 + i] = (p->data[row + 1] >> (8 * i)) & 0xFF;
            } else {
                payload[7 + i] = 0;
            }
            if (row + 2 < MAX_ROWS) {
                payload[11 + i] = (p->data[row + 2] >> (8 * i)) & 0xFF;
            } else {
                payload[11 + i] = 0;
            }
        }

        success = false;
        while (!success) {
            success = SimbleeCOM.send(payload, sizeof(payload));
        }
        row += 3;
    }

    // Start listening for packets again
    SimbleeCOM.startReceive();
    d("Transferred Page " + String(pageNumber));
}

void sendROM()
{
    // TODO send ROM page counter
    for (int i = STORAGE_FLASH_PAGE; i <= romManager.config.pageCounter; i--) {
        sendROMPage(i);
    }
    // TODO send ROM transfer complete message
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
 * Write a ROM reset row
 */
void resetROM()
{
    writeDataRow(ROW_RESET);
}
