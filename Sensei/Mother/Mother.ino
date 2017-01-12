#include "config.h"
#include "Common\debug.h"
#include "Common\config.h"
#include "Common\rtc.h"
#include "Common\radio_common.h"
#include "Common\accel.h"
#include "Common\rom.h"
#include "Common\command.h"
#include "Common\Wire\Wire.h"
#include "heatshrink_decoder.h"

#include <SimbleeCOM.h>

unsigned long deviceOnlineTime[NETWORK_SIZE];
bool RTC_FLAG = false;

/*
 * Send request for ROM data to network nodes
 */
void RequestROMFull(uint8_t id)
{
    char payload[] = {RADIO_REQUEST_FULL, id};
    SimbleeCOM.send(payload, sizeof(payload));
    Serial.println("D " + String(id));
}

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

bool transferInProgress = false;
unsigned int transferEsn = 0;
int transferPacketsLeft = 0;
int transferBytes = 0;
int transferBufferIndex = 0;
uint8_t transferCounter = 0;
uint8_t transferID = 0xFF;
#define input_size (1024)
#define decomp_sz (input_size + (input_size / 2) + 4)
uint8_t transferBuffer[decomp_sz];
uint8_t decomp[input_size];
static heatshrink_decoder hsd;
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

        if (esn != transferEsn) {
            Serial.print("Error: Received packet from ");
            PrintHexInt(esn);
            Serial.print(" during transfer from ");
            PrintHexInt(transferEsn);
            return;
        }

        if ((uint8_t)payload[0] != transferCounter) {
            Serial.print("Error: expected packet #");
            PrintHexByte(transferCounter);
            Serial.print(", but got #");
            PrintHexByte(payload[0]);
            transferInProgress = false; // TODO: packet retries
            return;
        }

        transferCounter++;

        for (int i = 1; i < len; i++) {
            // Hex in debug mode; binary in release mode
            transferBuffer[transferBufferIndex++] = payload[i];
            PrintHexByte(payload[i]);
        }
        return;

        transferPacketsLeft--;
        if (transferPacketsLeft == 0) {
            heatshrink_decoder_reset(&hsd);

            size_t count = 0;
            uint32_t sunk = 0;
            uint32_t polled = 0;

            d(transferBytes);
            while (sunk < transferBytes) {
                HSD_sink_res sres;
                sres = heatshrink_decoder_sink(&hsd, &transferBuffer[sunk], transferBytes - sunk, &count);
                if (sres < 0) {
                    Serial.print("Error: sres=");
                    Serial.println(sres);
                }
                sunk += count;
                if (sunk == transferBytes) {
                    HSD_finish_res fres = heatshrink_decoder_finish(&hsd);
                    if (fres != HSDR_FINISH_MORE) {
                        Serial.print("Error: fres=");
                        Serial.println(fres);
                    }
                }

                HSD_poll_res pres;
                do {
                    pres = heatshrink_decoder_poll(&hsd, &decomp[polled], decomp_sz - polled, &count);
                    if (pres < 0) {
                        Serial.print("Error: pres=");
                        Serial.println(pres);
                    }
                    polled += count;
                } while (pres == HSDR_POLL_MORE);
                if (pres != HSDR_POLL_EMPTY) {
                    Serial.print("Error: pres=");
                    Serial.println(pres);
                }
                if (sunk == transferBytes) {
                    HSD_finish_res fres = heatshrink_decoder_finish(&hsd);
                    if (fres != HSDR_FINISH_DONE) {
                        Serial.print("Error: fres=");
                        Serial.println(fres);
                    }
                }

                if (polled > input_size) {
                    Serial.println("Error: Decompressed data is larger than original input");
                    d(polled);
                    d(input_size);
                    break;
                }
            }
            if (polled != input_size) {
                Serial.println("Error: Decompressed length does not match original input length");
                d(polled);
                d(input_size);
            }

            for (int i = 0; i < 1024; i++) {
                // Hex in debug mode; binary in release mode
                PrintByte(decomp[i]);
            }
            Serial.println();

            transferInProgress = false;
            transferEsn = 0xFF;
            transferPacketsLeft = 0;
            transferBytes = 0;
            transferCounter = 0;
            transferBufferIndex = 0;
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
        d("Page number: " + String((uint8_t)payload[2]));
        dn("Device: ");
        PrintHexInt(esn);
        d();
        transferEsn = esn;
        transferBytes = (uint8_t)payload[3] << 8 | (uint8_t)payload[4];
        transferPacketsLeft = (uint8_t)payload[5];
        transferCounter = 0;
        transferID = id;
        transferBufferIndex = 0;
        memset(transferBuffer, 0, decomp_sz);
        memset(decomp, 0, input_size);
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