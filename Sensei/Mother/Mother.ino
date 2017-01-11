#define MOTHER_NODE
#define STUDENT_TRACKER false
#define LESSON_TRACKER false
#define REGION_TRACKER false

#include "Common\debug.h"
#include "Common\config.h"
#include "Common\rtc.h"
#include "Common\radio_common.h"
#include "Common\accel.h"
#include "Common\rom.h"
#include "Common\command.h"
#include "Common\Wire\Wire.h"

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
    d(romManager.config.deviceID);

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

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi)
{
    dn("Msg ");
    dn(len);
    dn(" ");
    dn(esn);
    dn("(");
    dn(rssi);
    dn(") ");
    for (int i = 0; i < len; i++) {
        PrintByteDebug(payload[i]);
    }
    d("");

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

    if (len == 15) {
        // TODO Print esn, this will help the app tie the rows messages together

        // Parse as RADIO_RESPONSE_ROWS packet and return
        for (int i = 0; i < len; i++) {
            // Hex in debug mode; binary in release mode
            PrintByte(payload[i]);
        }
        Serial.println();
        return;
    }

    uint8_t command = payload[0];
    uint8_t id = payload[1];

    // Over the air protocol, bytes:
    // 0: Command
    // 1: Device ID, if applicable
    // n: Data
    switch (command) {
    case RADIO_PROX_PING:
        // Proximity beacon
        dn("Device online: ");
        d(id);

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

    d("Broadcast RTC Time: ");
    timer.displayDateTime();
}