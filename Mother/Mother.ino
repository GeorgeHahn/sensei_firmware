#include "config.h"
#include "src/Common/debug.h"
#include "src/Common/config.h"
#include "src/Common/rtc.h"
#include "src/Common/radio_common.h"
#include "src/Common/accel.h"
#include "src/Common/rom.h"
#include "src/Common/command.h"
#include "src/Common/Wire/Wire.h"

#include <SimbleeCOM.h>

unsigned long deviceOnlineTime[NETWORK_SIZE];
bool RTC_FLAG = false;

void setup()
{
    Serial.begin(BAUD_RATE);
    pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
    Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);
    SimbleeCOM.txPowerLevel = 4;
    RandomSeed();
    Wire.speed = 400;
    Wire.beginOnPins(14, 13);

    delay(1000);
    d("");
    d("Mother node");

    DisableRTCInterrupt();
    startBroadcast();
}

void loop()
{
    delay(5);
    synchronizeTime();
    InterpretCommand();
}

/*
 * Outputs whether each device is online
 */

void printOnlineDevices()
{
    Serial.print("O" + String(":"));
    for (int i = 0; i < NETWORK_SIZE; i++) {
        Serial.print(String(millis() - deviceOnlineTime[i] < SECONDS_TO_TRACK * 1000 && millis() > SECONDS_TO_TRACK * 1000) + ",");
    }
    Serial.println();
}

int RTC_Interrupt(uint32_t ulPin)
{
    RTC_FLAG = true;
    return 0;
}

// These don't need to be a bitfield
#define HEADER_TYPE_LENGTH (0x00)
#define HEADER_TYPE_ERROR (0x01)
#define HEADER_TYPE_BATTERY (0x02)
#define HEADER_TYPE_PAGE_COUNT (0x03);

/* 
 * Print header for data row
 *
 * Header format: 3 bytes
 *  value: mask 0x7FF << 0 (11 bits)
 *  ID: mask 0xFF << 11 (8 bits)
 *      device id (0-63) - may grow in the future
 *  compression: 1 bit (mask 0x1 << 19 (1 bit))
 *  header type: 2 bits (mask 0x3 << 20 (2 bits))
 *      length: value field is 0-1023
 *      error: error number may be set in the value field (currently unimplemented)
 *      battery: level is 8 bits (in value field)
 *      page count: number of used pages set in value field (0-128 currently)
 *  reserved: top 2 bits
 */
void PrintPageHeader(int ID, uint8_t headerType, int value, bool compressionFlag)
{
    uint32_t header = 0;
    header = (value & 0x7FF) |
             (ID & 0xFF) << 11 |
             (compressionFlag & 0x01) << 19 |
             (headerType & 0x03) << 20;
    PrintHexByte((header >> 16) & 0xFF);
    PrintHexByte((header >> 8) & 0xFF);
    PrintHexByte(header & 0xFF);
}

bool transferInProgress = false;
unsigned int transferEsn = 0;
int transferPacketsLeft = 0;
int transferBytes = 0;
int transferBufferIndex = 0;
uint8_t transferCounter = 0;
uint8_t transferID = 0xFF;
#define input_size (1024)
uint8_t transferBuffer[input_size];
void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi)
{
    if (len < 2) {
        dn("Invalid payload");
        dn(esn);
        dn("(");
        dn(rssi);
        dn(") ");
        for (int i = 0; i < len; i++) {
            PrintByteDebug(payload[i]);
        }
        d("");
        return;
    }

    // SimbleeCOM max packet size is 15 bytes
    // Parse as RADIO_RESPONSE_ROWS packet and return
    if (len == 15) {
        if (!transferInProgress) {
            return;
        }

        bool error = false;

        if (esn != transferEsn) {
            error = true;

#ifdef DEBUG
            Serial.print("Error: Received packet from ");
            PrintHexInt(esn);
            Serial.print(" during transfer from ");
            PrintHexInt(transferEsn);
#endif
        }

        if ((uint8_t)payload[0] != transferCounter) {
            error = true;
            transferInProgress = false; // TODO: packet retries

#ifdef DEBUG
            Serial.print("Error: expected packet #");
            PrintHexByte(transferCounter);
            Serial.print(", but got #");
            PrintHexByte(payload[0]);
#endif
        }

        if (!error) {
            transferCounter++;

            for (int i = 1; i < len; i++) {
                // Hex in debug mode; binary in release mode
                transferBuffer[transferBufferIndex++] = payload[i];
            }

            transferPacketsLeft--;
        }

        if (transferPacketsLeft == 0 || error) {
            // If error, print all of the bytes, then send an error header
            for (int i = 0; i < input_size; i++) {
                // Hex in debug mode; binary in release mode
                PrintByte(transferBuffer[i]);
            }
            Serial.println();

            if (error) {
                PrintPageHeader(transferID, HEADER_TYPE_ERROR, 0 /* TODO: error code */, false);
            }

            transferInProgress = false;
            transferEsn = 0xFF;
            transferPacketsLeft = 0;
            transferBytes = 0;
            transferCounter = 0;
            transferBufferIndex = 0;
            transferID = 0;
        }
    }

    uint8_t command = payload[0];
    uint8_t id = payload[1];

    // Over the air protocol, bytes:
    // 0: Command
    // 1: Device ID, if applicable
    // n: Data
    switch (command) {
    case RADIO_START_NEW_TRANSFER:
        if (esn != transferEsn && transferInProgress) {
            Serial.println("ERROR: Transfer already in progress");
        }
        transferInProgress = true;
        d("Page number: " + String((uint8_t)payload[2]));
        dn("Device: ");
        dn("Full pages: ");
        d(payload[6]);
        d(esn);
        d();

        transferEsn = esn;
        transferBytes = (uint8_t)payload[3] << 8 | (uint8_t)payload[4];
        transferPacketsLeft = (uint8_t)payload[5];
        transferCounter = 0;
        transferID = id;
        transferBufferIndex = 0;

        // Print row length
        PrintPageHeader(transferID, HEADER_TYPE_LENGTH, transferBytes, false);

        // Zero our buffer (not strictly necessary)
        memset(transferBuffer, 0, 1024);
        break;

    case RADIO_RESPONSE_BATTERY:
        uint8_t batterylevel = (uint8_t)payload[2];

        // Print battery level
        PrintPageHeader(id, HEADER_TYPE_BATTERY, batterylevel, false);
        break;

    case RADIO_PROX_PING:
        // Proximity beacon
        dn("Device online: ");
        d(id);

        if (id >= NETWORK_SIZE) {
            Serial.println("Device ID out of range: " + String(id));
            return;
        }

        // Mother node online device tracking
        deviceOnlineTime[id] = millis();
        break;

    case RADIO_RESPONSE_ROWS:
        // Handled above; all 15B packets are interpreted as RADIO_RESPONSE_ROWS packets

        // Bytes
        //  command
        //  Page
        //  Row
        //  Data[]

        // Assumption: Only one shoe sensor is sending rows at once
        // for (int i = 1; i < len; i++) {
        //     // Hex in debug mode; binary in release mode
        //     PrintByte(payload[i]);
        // }
        // Serial.println();
        break;

    case RADIO_RESPONSE_PAGEINFO:
        PrintPageHeader(id, HEADER_TYPE_PAGE_COUNT, payload[2], false);
        break;

    case RADIO_RESPONSE_COMPLETE:
        break;

    // Mother doesn't care about these packets
    case RADIO_SHARED_TIME:
    case RADIO_REQUEST_FULL:
    case RADIO_REQUEST_PARTIAL:
    case RADIO_REQUEST_ERASE:
    case RADIO_REQUEST_SLEEP:
        break;

    default:
        dn("Invalid payload");
        dn(esn);
        dn("(");
        dn(rssi);
        dn(") ");
        for (int i = 0; i < len; i++) {
            PrintByteDebug(payload[i]);
        }
        d("");
        break;
    }
}

void synchronizeTime()
{
    if (!RTC_FLAG) {
        return;
    }

    RTC_FLAG = false;

    if (!timer.isTimeSet) {
        return;
    }

    DS3231_get(&rtcTime);
    timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
                         rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);

    SendTime(timer.t.month, timer.t.date, timer.t.year,
             timer.t.day, timer.t.hours, timer.t.minutes,
             timer.t.seconds);

    dn("Broadcasting RTC Time: ");
    timer.displayDateTime();
}
