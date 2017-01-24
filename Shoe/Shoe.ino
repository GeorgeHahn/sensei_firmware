#define STUDENT_TRACKER true
#define LESSON_TRACKER false
#define REGION_TRACKER false

#include "config.h"
#include "src/Common/debug.h"
#include "src/Common/config.h"
#include "src/Common/rtc.h"
#include "src/Common/radio_common.h"
#include "src/Common/accel.h"
#include "src/Common/rom.h"
#include "src/Common/command.h"
#include "src/Common/battery.h"

#include <SimbleeCOM.h>
#include "src/Common/Wire/Wire.h"
#include "shoe_rom.h"

// TODO: If after 4pm, set an alarm for the next morning at 9am and sleep until then
//	Use RTC alarm to wake up at the beginning of START_HOUR:START_MINUTE
//	Lesson tracker & region tracker may be able to get away without storing any data

// Boolean on whether to collect data
volatile bool collectData;

void setup()
{
    pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
    SimbleeCOM.txPowerLevel = TX_POWER_LEVEL;

    // nRF provides an internal source of randomness
    RandomSeed();

    // Set I2C to 400KHz
    Wire.speed = 400;
    Wire.beginOnPins(14, 13);

    Serial.begin(BAUD_RATE);

    delay(500);
    d("");
    if (REGION_TRACKER) {
        d("Region tracker");
    }
    if (LESSON_TRACKER) {
        d("Lesson tracker");
    }
    if (STUDENT_TRACKER) {
        d("Student sensor");
    }
    d(romManager.config.deviceID);

    if (romManager.config.deviceID == 0x00) {
        Serial.println("WARNING: DEVICE ID NOT SET");
    }

    // Put a mark in ROM (TODO: do we care?)
    resetROM();

    // Don't do anything after powerup until we receive a time.
    // Sleeps an odd number of MS to ensure time will be picked up after a few cycles
    d("Waiting for time");
    startBroadcast();
    while (!timer.isTimeSet) {
        delay(5);
        InterpretCommand();

        // Don't try to sleep if the deviceID isn't set
        // (going to sleep may mess up the serial console; fresh devices need console access to set the ID)
        if (romManager.config.deviceID != 0x00) {
            delay(600); // TODO may want to loop InterpretCommand during this
            Simblee_ULPDelay(MILLISECONDS(800));
        }
    }
    stopBroadcast();
    d("Time set.");

    if (STUDENT_TRACKER || LESSON_TRACKER) {
        setupAccelerometer();
    }

    if (!USE_SERIAL_MONITOR) {
        Serial.end();
    }

    EnableRTCInterrupt();
    Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);
}

void SendPing()
{
    char payload[] = {RADIO_PROX_PING, romManager.config.deviceID};

    bool success = false;
    while (!success) {
        success = SimbleeCOM.send(payload, sizeof(payload));
    }
}

void SendBatteryLevel()
{
    // Currently, only the student tracker hardware is capable of performing battery measurements
    if (!STUDENT_TRACKER) {
        return;
    }
    char payload[] = {RADIO_RESPONSE_BATTERY, romManager.config.deviceID, get_battery_pct()};

    bool success = false;
    while (!success) {
        success = SimbleeCOM.send(payload, sizeof(payload));
    }
}

uint8_t ping_transmit_delay;
void loop()
{
    if (collectData) {
        collectData = false;
        d("Collect");
        synchronizeTime();
        if (!REGION_TRACKER) {
            enableAccelerometer();
            readAcc();
            disableAccelerometer();
        }
        startBroadcast();

        ping_transmit_delay = random(MS_SEND_DELAY_MIN, MS_SEND_DELAY_MAX);
        delay(ping_transmit_delay);
        SendPing();
        delay(MS_TO_COLLECT - ping_transmit_delay);

        stopBroadcast();
        writeData();
        d("Collected");
    }

    dn("_");
    Serial.flush();
    Simblee_ULPDelay(SECONDS(1));

    // TODO: It would be really nice if we knew why this call doesn't work as expected
    //Simblee_systemOff();
}

int RTC_Interrupt(uint32_t ulPin)
{
    Simblee_resetPinWake(ulPin);

    // TODO: This is an expensive call and should be refactored or eliminated
    timer.NextSecond();

    // Collect data every few seconds if we're in the data collection period
    if (timer.t.seconds % 10 == 0) {
        if (timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
            collectData = true;
        }
        Serial.println("");
    }

    dn("-");

    return 0;
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi)
{
    // dn("Msg ");
    // dn(len);
    // dn(" ");
    // dn(esn);
    // dn("(");
    // dn(rssi);
    // dn(") ");
    // for (int i = 0; i < len; i++)
    //     PrintByteDebug(payload[i]);
    // d("");

    if (len < 2) {
        dn("Invalid payload");
        dn(esn);
        dn("(");
        dn(rssi);
        dn(") ");
        for (int i = 0; i < len; i++)
            PrintByteDebug(payload[i]);
        d("");
        return;
    }

    // The only 15B long messages are RADIO_RESPONSE_ROWS messages, which we should ignore
    if (len == 15) {
        d("Ignoring 15B message");
        return;
    }

    uint8_t command = payload[0];
    uint8_t id = payload[1];
    bool id_match = (id == romManager.config.deviceID);

    // Over the air protocol, bytes:
    // 0: Command
    // 1: Device ID, if applicable
    // n: Data
    switch (command) {
    case RADIO_PROX_PING:
        // Proximity beacon
        // Recording RSSI data
        dn("Device online: ");
        dn(id);
        dn(" RSSI ");
        dn(rssi);

        if (timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) { // TODO: Move this out to a flag
            rssiTotal[id] += rssi;
            rssiCount[id]++;
            newData = true;
        }
        break;

    case RADIO_SHARED_TIME:
        // Time sharing
        d("Time received");
        noInterrupts();
        setRTCTime((int)payload[1], (int)payload[2], (int)payload[3], (int)payload[4], (int)payload[5], (int)payload[6], (int)payload[7]);
        timer.totalSecondsElapsed = 1;
        timer.secondsElapsed = 1;
        synchronizeTime();

        EnableRTCInterrupt();
        interrupts();
        d("Done");
        break;

    case RADIO_REQUEST_PAGEINFO:
        if (!id_match) {
            return;
        }

        sendPageInfo();
        break;

    case RADIO_REQUEST_FULL:
        dn("Data transfer");
        d(id);
        if (!id_match) {
            return;
        }

        SendBatteryLevel();
        sendROM();
        break;

    case RADIO_REQUEST_COMPRESSED:
        dn("Compressed data transfer");
        d(id);
        if (!id_match) {
            return;
        }

        SendBatteryLevel();
        sendROM_heatshrink();
        break;

    case RADIO_REQUEST_PARTIAL:
        if (!id_match) {
            return;
        }
        //transferPage = (uint8_t)payload[1];
        //transferRow = (uint8_t)payload[2];
        //transferRowsLeft = (uint8_t) payload[3];
        // length

        sendROMPage((uint8_t)payload[1]);
        break;

    case RADIO_REQUEST_ERASE:
        if (!id_match) {
            return;
        }
        remoteEraseROM();
        break;

    case RADIO_REQUEST_SLEEP_ERASE:
        if (!id_match) {
            return;
        }
        remoteEraseROM();
    // fall through to sleep

    case RADIO_REQUEST_SLEEP:
        if (!id_match) {
            return;
        }
        // TODO: Go to sleep until tomorrow morning
        // Set RTC alarm for tomorrow morning
        // Simblee_SystemOff
        break;

    case RADIO_ENTER_OTA_MODE:
        if (!id_match) {
            return;
        }

        // WARNING: Simblee OTA stomps all over user flash pages
        // See http://forum.rfduino.com/index.php?topic=1273.25
        //
        // Note: there are some reports that activity on pin 5 at boot can trigger OTA
        //ota_bootloader_start();
        break;

    case RADIO_REQUEST_NEXT_PAGE:
        if (!id_match) {
            return;
        }

        ready_for_next_page = true;
        break;

    // Ignore other devices' responses
    case RADIO_RESPONSE_ROWS:
    case RADIO_RESPONSE_COMPLETE:
        break;
    default:
        dn("Invalid payload");
        dn(esn);
        dn("(");
        dn(rssi);
        dn(") ");
        for (int i = 0; i < len; i++)
            PrintByteDebug(payload[i]);
        d("");
        break;
    }
}

/*
 * Synchonrizes the node's time with the RTC time
 */
void synchronizeTime()
{
    DS3231_get(&rtcTime);
    timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
                         rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);
    Serial.print("RTC Time: ");
    timer.displayDateTime();
}
