#define STUDENT_TRACKER true
#define LESSON_TRACKER false
#define REGION_TRACKER false

#include "Common\debug.h"
#include "Common\config.h"
#include "Common\rtc.h"
#include "Common\radio_common.h"
#include "Common\accel.h"
#include "Common\rom.h"
#include "Common\command.h"

#include <SimbleeCOM.h>
#include <Wire.h>

// TODO

// TODO: If after 4pm, set an alarm for the next morning at 9am and sleep until then

//	Use RTC alarm to wake up at the beginning of START_HOUR:START_MINUTE
//	Increase I2C speed to 400kHz
//	Why are we including the DS3231 library if we're not using it?

//	Lesson tracker & region tracker may be able to get away without storing any data
bool RTC_FLAG = false;

void setup()
{
    pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
    Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);

    SimbleeCOM.txPowerLevel = TX_POWER_LEVEL;

    randomSeed(analogRead(UNUSED_ANALOG_PIN));

    Serial.setTimeout(5);
    enableSerialMonitor();

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

    setupSensor();

    if (STUDENT_TRACKER || LESSON_TRACKER) {
        setupAccelerometer();
    }
}

void SendPing()
{
    char payload[] = {RADIO_PROX_PING, romManager.config.deviceID};
    SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Interfaces with device to receive time or print ROM data
 */
void setupSensor()
{
    Serial.begin(BAUD_RATE);
    resetROM();
    stopInterrupt();
    startBroadcast();
    while (!timer.isTimeSet) {
        delay(5);
        InterpretCommand();

        acknowledgeTimeReceipt();
    }
    if (!USE_SERIAL_MONITOR) {
        Serial.end();
    }
}

uint8_t ping_transmit_delay;
void loop()
{
    synchronizeTime();

    if (collectData) {
        if (!REGION_TRACKER) {
            enableAccelerometer();
            readAcc();
            disableAccelerometer();
        }

        timer.updateTime();
        startBroadcast();

        ping_transmit_delay = random(MS_SEND_DELAY_MIN, MS_SEND_DELAY_MAX);
        delay(ping_transmit_delay);
        SendPing();
        delay(MS_TO_COLLECT - ping_transmit_delay);

        stopBroadcast();
        writeData();

        Simblee_ULPDelay(INFINITE);
        // TODO: Try Simblee_systemOff(); instead.
        //   Much lower power usage, but no clue what the behavior is, if RAM is saved, or where execution goes after the pin callback. No docs.
    }
}

uint8_t collectData_count = 0;
int RTC_Interrupt(uint32_t ulPin)
{
    Simblee_resetPinWake(ulPin);

    timer.totalSecondsElapsed++;
    timer.secondsElapsed++;

    dn("Time: ");
    timer.displayDateTime();

    // Collect data every few seconds if we're in the data collection period
    collectData_count++;
    if (collectData_count > 10) {
        collectData_count = 0;
        if (timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
            collectData = true;
        }
    }

    // Update internal timer from RTC every few minutes
    RTC_FLAG = (timer.secondsElapsed >= MINUTES_BETWEEN_SYNC * 60 &&
                !timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) ||
               !timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
                   !timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE) ||
               collectData;
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
    for (int i = 0; i < len; i++)
        PrintByteDebug(payload[i]);
    d("");

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

    uint8_t command = payload[0];
    uint8_t id = payload[1];

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
        stopInterrupt();
        timer.setInitialTime((int)payload[1], (int)payload[2], (int)payload[3], (int)payload[4], (int)payload[5], (int)payload[6], (int)payload[7]);
        timer.totalSecondsElapsed = 1;
        timer.secondsElapsed = 1;
        setRTCTime();
        startInterrupt();
        break;

    case RADIO_REQUEST_FULL:
        d("Data transfer");
        sendROMResponse();
        break;

    case RADIO_REQUEST_PARTIAL:
        transferPage = (uint8_t)payload[1];
        transferRow = (uint8_t)payload[2];
        //transferRowsLeft = (uint8_t) payload[3];
        // length
        break;

    case RADIO_REQUEST_ERASE:
        remoteEraseROM();
        break;

    case RADIO_RQUEST_SLEEP:
        // TODO: Go to sleep until tomorrow morning
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

void acknowledgeTimeReceipt()
{
    discoveryTime = millis();
    while (!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
           !timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
        delay(100);
        synchronizeTime();
    }
}

/*
 * Synchonrizes the node's time with the RTC time
 */
void synchronizeTime()
{
    if (RTC_FLAG) {
        DS3231_get(&rtcTime);
        timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
                             rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);
        RTC_FLAG = false;
        Serial.print("RTC Time: ");
        timer.displayDateTime();
    }
}