#include "shoe_rom.h"

#include <SimbleeCOM.h>
#include "src/Common/rom.h"
#include "src/Common/rtc.h"
#include "src/Common/accel.h"
#include "src/Common/radio_common.h"
#include "src/Common/debug.h"
#include "heatshrink_encoder.h"

static heatshrink_encoder hse;
volatile bool ready_for_next_page;

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
        // TODO: Share timestamp between multiple events
        // Maybe that needs to be a different row type?
        // 0bRRRRRRRIIIIIIIIRRRRRRRIIIIIIII // Event one, event two

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

#define PAGE_SZ 1024
#define COMP_SZ (PAGE_SZ + (PAGE_SZ / 2) + 4)
uint8_t comp[COMP_SZ];
void sendROMPage_heatshrink(uint8_t pageNumber)
{
    d("Transferring Page " + String(pageNumber));
    data *p = (data *)ADDRESS_OF_PAGE(pageNumber);
    char payload[15];
    bool success = false;

    // Don't listen for packets while we're sending ROM
    SimbleeCOM.stopReceive();

    heatshrink_encoder_reset(&hse);
    memset(comp, 0, COMP_SZ);

    size_t count = 0;
    uint32_t sunk = 0;
    uint32_t polled = 0;
    while (sunk < PAGE_SZ) {
        //ASSERT(heatshrink_encoder_sink(&hse, &input[sunk], input_size - sunk, &count) >= 0);
        heatshrink_encoder_sink(&hse, (uint8_t *)&p->data[sunk], PAGE_SZ - sunk, &count);
        sunk += count;
        if (sunk == PAGE_SZ) {
            //ASSERT_EQ(HSER_FINISH_MORE, heatshrink_encoder_finish(&hse));
            heatshrink_encoder_finish(&hse);
        }

        HSE_poll_res pres;
        do { /* "turn the crank" */
            pres = heatshrink_encoder_poll(&hse, &comp[polled], COMP_SZ - polled, &count);
            //ASSERT(pres >= 0);
            polled += count;
        } while (pres == HSER_POLL_MORE);
        //ASSERT_EQ(HSER_POLL_EMPTY, pres);
        if (polled >= COMP_SZ)
            d("compression should never expand that much");
        if (sunk == PAGE_SZ) {
            //ASSERT_EQ(HSER_FINISH_DONE, heatshrink_encoder_finish(&hse));
            heatshrink_encoder_finish(&hse);
        }
    }

    //d("Waiting to send next page");
    // Tell mother what page & how big it is after compression
    memset(payload, 0, 15);
    payload[0] = RADIO_START_NEW_TRANSFER;
    payload[1] = romManager.config.deviceID;
    payload[2] = pageNumber;                    // current page
    payload[3] = polled >> 8 & 0xFF;            // byte count high
    payload[4] = polled & 0xFF;                 // byte count low
    payload[5] = ((polled - 1) / 14) + 1;       // packet count
    payload[6] = romManager.config.pageCounter; // total page count

    success = false;
    while (!success) {
        success = SimbleeCOM.send(payload, 7);
    }
    //d("Sending next page");

    delay(5);

    count = 0;
    uint8_t counter = 0;
    while (count < polled) {
        payload[0] = counter;
        for (int i = 1; i < 15; i++) {
            payload[i] = comp[count++];
        }

        success = false;
        while (!success) {
            success = SimbleeCOM.send(payload, sizeof(payload));
        }
        counter++;
    }

    // Start listening for packets again
    SimbleeCOM.startReceive();
    d("Transferred Page " + String(pageNumber) + " compressed to " + String(polled));
}

void sendROM_heatshrink()
{
    for (int i = STORAGE_FLASH_PAGE; i >= STORAGE_FLASH_PAGE - romManager.config.pageCounter; i--) {
        sendROMPage_heatshrink(i);
    }
}

/*
 * Send response for ROM data to mother node
	//  command
	//  Page
	//  Row
	//  Data[]
 */

#define size 1024
void sendROMPage(uint8_t pageNumber)
{
    d("Transferring Page " + String(pageNumber));
    data *p = (data *)ADDRESS_OF_PAGE(pageNumber);
    char payload[15];
    bool success = false;

    // Don't listen for packets while we're sending ROM
    SimbleeCOM.stopReceive();

    //d("Waiting to send next page");
    // Tell mother what page & how big it is after compression
    memset(payload, 0, 15);
    payload[0] = RADIO_START_NEW_TRANSFER;
    payload[1] = romManager.config.deviceID;
    payload[2] = pageNumber;                    // current page
    payload[3] = size >> 8 & 0xFF;              // byte count high
    payload[4] = size & 0xFF;                   // byte count low
    payload[5] = ((size - 1) / 14) + 1;         // packet count
    payload[6] = romManager.config.pageCounter; // total page count

    // TODO: All SimbleeCOM send calls should check for success (!success => queue is full)
    success = false;
    while (!success) {
        success = SimbleeCOM.send(payload, 7);
    }
    //d("Sending next page");

    delay(5);

    uint16_t count = 0;
    uint8_t counter = 0;
    while (count < size) {
        payload[0] = counter;
        for (int i = 1; i < 15; i++) {
            payload[i] = p->data[count++];
        }

        success = false;
        while (!success) {
            success = SimbleeCOM.send(payload, sizeof(payload));
        }
        counter++;
    }

    // Start listening for packets again
    SimbleeCOM.startReceive();
    d("Transferred Page " + String(pageNumber));
}

void sendROM()
{
    d("pageCount = " + String(romManager.config.pageCounter));
    for (int i = STORAGE_FLASH_PAGE; i >= romManager.config.pageCounter; i--) {
        d("sending page: " + String(i));
        sendROMPage(i);
    }
}

void sendPageInfo()
{
    char payload[] = {RADIO_RESPONSE_PAGEINFO, romManager.config.deviceID, romManager.config.pageCounter};
    bool success = false;
    while (!success) {
        success = SimbleeCOM.send(payload, sizeof(payload));
    }
}
