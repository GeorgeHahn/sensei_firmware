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
//	Replace usage of deviceID with a command value

//	Use RTC alarm to wake up at the beginning of START_HOUR:START_MINUTE
//	Increase I2C speed to 400kHz
//	Why are we including the DS3231 library if we're not using it?

//	Lesson tracker shouldn't store data

bool RTC_FLAG = false;

void setup() {
	pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
	Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);
	
	SimbleeCOM.txPowerLevel = TX_POWER_LEVEL;
	
	randomSeed(analogRead(UNUSED_ANALOG_PIN));
	
	Serial.setTimeout(5);
	enableSerialMonitor();
	
	d("");
	if(REGION_TRACKER) {
		d("Region tracker");
	}
	if(LESSON_TRACKER) {
		d("Lesson tracker");
	}
	if(STUDENT_TRACKER) {
		d("Student sensor");
	}
	
	setupSensor();
	
	if (STUDENT_TRACKER || LESSON_TRACKER) {
		setupAccelerometer();
	}
}

/*
 * Interfaces with device to receive time or print ROM data
 */
void setupSensor() {
	Serial.begin(BAUD_RATE);
	resetROM();
	stopInterrupt();
	startBroadcast();
	while (!timer.isTimeSet) {
		if (transferROM) {
			sendROMResponse();
		} else if (erase) {
			remoteEraseROM();
		}
		delay(5);
		
		InterpretCommand();
		acknowledgeTimeReceipt();
	}
	if (!USE_SERIAL_MONITOR) {
		Serial.end();
	}
}

void loop() {
	synchronizeTime();
	
	if (collectData) {
		startBroadcast();
		delay(random(MS_SEND_DELAY_MIN, MS_SEND_DELAY_MAX));
		if (!REGION_TRACKER) {
			enableAccelerometer();
			readAcc();
			timer.updateTime();
			char payload[] = {};
			SimbleeCOM.send(payload, sizeof(payload));
		}
	} else {
		if (!REGION_TRACKER) {
			disableAccelerometer();
		}
		stopBroadcast();
		writeData();
		Simblee_ULPDelay(INFINITE);
	}
}

uint8_t collectData_count = 0;
int RTC_Interrupt(uint32_t ulPin) {
	// TODO: If after 4pm, set an alarm for the next morning at 9am and sleep until then
	
	timer.totalSecondsElapsed++;
	timer.secondsElapsed++;
	
	dn("Time: "); timer.displayDateTime();
	
	collectData_count++;
	if(collectData_count > 10) {
		collectData = timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE);
		collectData_count = 0;
	}
	
	RTC_FLAG
 = (timer.secondsElapsed >= MINUTES_BETWEEN_SYNC * 60 &&
					!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) ||
				!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
					!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE) ||
				collectData;
	return 0;
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi) {
	dn("Msg "); dn(len); dn(" "); dn(esn); dn("("); dn(rssi); dn(") "); for(int i = 0; i < len; i++) PrintHexByte(payload[i]); d("");
	
	uint8_t command = (len > 0) ? payload[0] : COMMAND_DUMMY_PROX;
	
	// Over the air protocol:
	// Byte 0:
	// 0: 
	// 1-50: Is a proximity beacon
	// 255 down: Is a packet
	if(command == 0) {
		d("Data transfer");
		erase = ((int) payload[1] == (char) -1) && ((int) payload[2] == (char) -1);
		transferROM = ((int) payload[1] != (char) -1) && ((int) payload[2] != (char) -1);
		transferPage = transferROM ? (int) payload[1] : transferPage;
		transferRow = transferROM ? (int) payload[2] : transferRow;
		if (romManager.loadedPage != transferPage && 
			transferPage >= LAST_STORAGE_PAGE) {
			romManager.loadPage(transferPage);
		}
	} else if(command < 50) {
		// Proximity beacon
		// 0xID
		
	} else switch(command) {
		case COMMAND_DUMMY_PROX:
			// Recording RSSI data
			uint8_t index = getDeviceIndex(esn);
			dn("Device online: "); dn(esn); dn(" RSSI "); dn(rssi); d(index);
			
			if(timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
				rssiTotal[index] += rssi;
				rssiCount[index]++;
				newData = true;
			}
			break;
		case COMMAND_SHARED_TIME:
			// Time sharing
			d("Time received");
			timer.setInitialTime((int) payload[1], (int) payload[2], (int) payload[3], (int) payload[4],(int) payload[5], (int) payload[6], (int) payload[7]);
			timer.totalSecondsElapsed = 1;
			timer.secondsElapsed = 1;
			setRTCTime();
			startInterrupt();
		case COMMAND_RESPONSE_ROWS:
			break;
		default:
			dn("Invalid payload"); dn(esn); dn("("); dn(rssi); dn(") "); for(int i = 0; i < len; i++) PrintHexByte(payload[i]); d("");
			break;
	}
}

void acknowledgeTimeReceipt() {
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
void synchronizeTime() {
	if (RTC_FLAG
) {
		DS3231_get(&rtcTime);
		timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
							rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);
		RTC_FLAG
	 = false;
		Serial.print("RTC Time: ");
		timer.displayDateTime();
	}
}