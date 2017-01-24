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

void ProcessPacket(unsigned int esn, const uint8_t payload, int len, int rssi);

unsigned long deviceOnlineTime[NETWORK_SIZE];
bool RTC_FLAG = false;

typedef struct packet {
    unsigned long millis;
    unsigned int esn;
    uint8_t data[15];
    int len;
    int rssi;
} packet;

#define PACKET_BUFFER_SIZE 256

packet packets[PACKET_BUFFER_SIZE];
unsigned int packetsHead;
unsigned int packetsTail;
unsigned int packetsCount;

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
    if (packetsCount == 0) {
        delay(5);
        synchronizeTime();
        InterpretCommand();
    } else {
        while (packetsCount > 0) {
            packet *p = &packets[packetsTail];
            ProcessPacket(p->esn, p->data, p->len, p->rssi);
            packetsCount--;
            packetsTail++;
            if (packetsTail >= PACKET_BUFFER_SIZE) {
                packetsTail = 0;
            }
        }
    }
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi)
{
    packet *p = &packets[packetsHead];
    p->millis = millis();
    p->esn = esn;
    memcpy(p->data, payload, len);
    p->len = len;
    p->rssi = rssi;
    packetsHead++;
    packetsCount++;
    if (packetsHead >= PACKET_BUFFER_SIZE) {
        packetsHead = 0;
    }
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
#define HEADER_TYPE_PAGE_COUNT (0x03)

/*
 * Print header for data row
 *
 * Header format: 3 bytes
 *  value: mask 0x7FF << 0 (11 bits)
 *  value2: mask 0xFF << 11 (8 bits)
 *  ID: mask 0xFF << 19 (8 bits)
 *      device id (0-63) - may grow in the future
 *  compression: 1 bit (mask 0x1 << 27 (1 bit))
 *  header type: 2 bits (mask 0x3 << 28 (2 bits))
 *      length: value field is 0-1023
                value2 field is 0-255
 *      error: error number may be set in the value field (currently unimplemented)
 *      battery: level is 8 bits (in value field)
 *      page count: number of used pages set in value field (0-128 currently)
 *  other bits: reserved
 */
void PrintPageHeader(int ID, uint8_t headerType, uint16_t value, uint8_t value2, bool compressionFlag)
{
    uint32_t header = 0;
    header = (value & 0x7FF) |
             (value2 & 0xFF) << 8 |
             (ID & 0xFF) << 19 |
             (compressionFlag & 0x01) << 27 |
             (headerType & 0x03) << 28;
    PrintByte((header >> 24) & 0xFF);
    PrintByte((header >> 16) & 0xFF);
    PrintByte((header >> 8) & 0xFF);
    PrintByte(header & 0xFF);
}

bool transferInProgress = false;
unsigned int transferEsn = 0;
int transferPacketsLeft = 0;
int transferBytes = 0;
int transferBufferIndex = 0;
uint8_t transferCounter = 0;
uint8_t pageCounter = 0;
uint8_t pageNumber = 0;
uint8_t transferID = 0xFF;
#define PAGE_SIZE (1024)
uint8_t transferBuffer[PAGE_SIZE];
void ProcessPacket(unsigned int esn, uint8_t *payload, int len, int rssi)
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
    if (len == 15 && transferInProgress) {

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

            // TODO: With counter, we can easily fill in the buffer and track chunks that have been Received, requesting missed chunks at the end
            // (that will req some protocol support for sending chunk offsets)
            for (int i = 1; i < len; i++) {
                transferBuffer[transferBufferIndex++] = payload[i];
            }
            transferPacketsLeft--;
        }

        // If complete, print all bytes and reset for the next transfer
        // If error, print all of the bytes, then send an error header and reset for the next transfer
        if (transferPacketsLeft == 0 || error) {
            // print error flag
            if (error) {
                d("error occured receiving page");
                PrintByte(0);
                PrintByte(0);
            } else {
                dn("page size: ");
                PrintByte(transferBytes >> 8);
                PrintByte(transferBytes & 0xff);
                d("");
                // print all of the bytes (hex in debug)
                PrintData(transferBuffer, transferBytes);
            }

            transferInProgress = false;
            transferEsn = 0xFF;
            transferPacketsLeft = 0;
            transferBytes = 0;
            transferCounter = 0;
            transferBufferIndex = 0;
            transferID = 0;
        }
        return;
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
        pageNumber = (uint8_t)payload[2];
        transferEsn = esn;
        transferBytes = (uint8_t)payload[3] << 8 | (uint8_t)payload[4];
        d("transferBytes: " + String(transferBytes));
        transferPacketsLeft = (uint8_t)payload[5];
        d("transferPacketsLeft: " + String(transferPacketsLeft));
        pageCounter = (uint8_t)payload[6];
        d("pageCounter: " + String(pageCounter));
        d();

        // Before the first page, print page count
        if (pageNumber == STORAGE_FLASH_PAGE) {
            dn("page count: ");
            PrintByte((STORAGE_FLASH_PAGE - pageCounter) + 1);
            d("");
        }

        dn("page number: ");
        PrintByte(STORAGE_FLASH_PAGE - pageNumber);
        d("");

        transferCounter = 0;
        transferID = id;
        transferBufferIndex = 0;

        // Zero our buffer (not strictly necessary)
        memset(transferBuffer, 0, 1024);
        break;

    case RADIO_RESPONSE_BATTERY:
        // Print battery level
        if (len == 4) {
            d("Battery level for " + String((uint8_t)payload[1]) + " is " + String((uint8_t)payload[2]) + "%");
        }
        //PrintPageHeader(id, HEADER_TYPE_BATTERY, (uint8_t)payload[2], 0, false);
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

        break;

    case RADIO_RESPONSE_PAGEINFO:
        PrintPageHeader(id, HEADER_TYPE_PAGE_COUNT, payload[2], 0, false);
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

#ifdef DEBUG
    dn("Broadcasting RTC Time: ");
    timer.displayDateTime();
#endif

    // Wait until sensors are listening to request data
    if (pendingDataRequestForSensorId > 0 && timer.t.seconds % 10 == 0) {
        delay(100);
        RequestROMFull(pendingDataRequestForSensorId);
        pendingDataRequestForSensorId = 0;
    }
}
