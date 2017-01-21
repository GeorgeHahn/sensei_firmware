#include <Arduino.h>
#include <SimbleeCOM.h>
#include "rom.h"
#include "rtc.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// RSSI total and count for each device
int rssiTotal[NETWORK_SIZE];
int rssiCount[NETWORK_SIZE];

// Boolean on whether radio stack is on
bool broadcasting;

// Boolean on whether received new data
volatile bool newData;
// Boolean on whether to collect data
volatile bool collectData;
// Time acknowledgment variable
unsigned long discoveryTime;

/*
 * Enables radio broadcasting
 */
void startBroadcast()
{
    if (broadcasting) {
        return;
    }

    SimbleeCOM.mode = LOW_LATENCY;
    SimbleeCOM.begin();
    broadcasting = true;
}

/*
 * Disables radio broadcasting
 */
void stopBroadcast()
{
    if (!broadcasting) {
        return;
    }

    SimbleeCOM.end();
    broadcasting = false;
}

/*
 * Send request for ROM data to network nodes
 */
void RequestFullData(uint8_t transferDevice)
{
    char payload[] = {RADIO_REQUEST_FULL, transferDevice};
    SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Send request for page information from a single device
 */
void RequestPageInfo(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_PAGEINFO, id};
    SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Send request for ROM data to network nodes
 */
void RequestPartialData(uint8_t transferDevice, uint8_t row, uint8_t length)
{
    char payload[] = {RADIO_REQUEST_PARTIAL, transferDevice, row, length};
    SimbleeCOM.send(payload, sizeof(payload));
}

void SendTime(uint8_t month, uint8_t date, uint8_t year, uint8_t day, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    char payload[] = {RADIO_SHARED_TIME, month, date, year, day, hours, minutes, seconds};
    SimbleeCOM.send(payload, sizeof(payload));
}

void RequestNextPage(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_NEXT_PAGE, id};
    SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Send request for ROM data to network nodes
 */
void RequestROMFull(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_FULL, id};
    SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Tell network node to go to sleep and erase its flash
 */
void RequestSleepErase(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_SLEEP_ERASE, id};
    SimbleeCOM.send(payload, sizeof(payload));
}
