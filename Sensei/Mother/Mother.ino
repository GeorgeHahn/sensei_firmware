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

#include <SimbleeCOM.h>
#include <Wire.h>

// TODO
//	Replace usage of deviceID with a command value

//	Use RTC alarm to wake up at the beginning of START_HOUR:START_MINUTE
//	Increase I2C speed to 400kHz
//	Why are we including the DS3231 library if we're not using it?

// Mother node device tracking
unsigned long deviceOnlineTime[NETWORK_SIZE];
bool RTC_FLAG = false;

void setup() {
	pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
	Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);
	SimbleeCOM.txPowerLevel = 4;
	randomSeed(analogRead(UNUSED_ANALOG_PIN));
	enableSerialMonitor();
	
	d("");
	d("Mother node");
	
	stopInterrupt();
	startBroadcast();
}

void loop() {
	delay(5);
	synchronizeTime();
	InterpretCommand();
}

/*
 * Outputs whether each device is online
 */

void printOnlineDevices() {
	Serial.print("O" + String(":"));
	for (int i = 0; i < NETWORK_SIZE; i++) {
		Serial.print(String(millis() - deviceOnlineTime[i] < SECONDS_TO_TRACK * 1000 && millis() > SECONDS_TO_TRACK * 1000) + ",");
	}
	Serial.println();
}

int RTC_Interrupt(uint32_t ulPin) {
	RTC_FLAG = true;
	timer.totalSecondsElapsed++;
	timer.secondsElapsed++;
	
	dn("Time: "); timer.displayDateTime();
	return 0;
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi) {
	dn("Msg "); dn(len); dn(" "); dn(esn); dn("("); dn(rssi); dn(") "); for(int i = 0; i < len; i++) PrintHexByte(payload[i]); d("");
	
	uint8_t command = payload[0];
	
	// Over the air protocol:
	// Byte 0:
	//	0: 
	//	1-50: Is a proximity beacon
	//	255 down: Is a packet
	if(command == 0) {
		
	} else if(command < 50) {
		// Proximity beacon
		dn("Device online: "); d(esn);

		// Mother node online device tracking
		deviceOnlineTime[getDeviceIndex(esn)] = millis();
	} else switch(command) {
		case COMMAND_SHARED_TIME:
			break;
		case COMMAND_RESPONSE_ROWS:
			if(command == transferDevice) {
				if(len == 2) {
					d("Last page");
					lastPage = true;
					lastTransferTime = millis();
				} else if(len == 3) {
					d("transferROM");
					transferROM = (((int) payload[1] != (char) -1) || ((int) payload[2] != (char) -1));
				}
			}
			break;
		default:
			dn("Invalid payload"); dn(esn); dn("("); dn(rssi); dn(") "); for(int i = 0; i < len; i++) PrintHexByte(payload[i]); d("");
			break;
			
		// TODO: transfer protocol
			/* 
		if(command == transferDevice) {
			if(len == 2) {
				d("Last page");
				lastPage = true;
				lastTransferTime = millis();
			} else if(len == 3) {
				d("transferROM");
				transferROM = (((int) payload[1] != (char) -1) || ((int) payload[2] != (char) -1));
			}
		}
		// ?
		if(transferROM && len == 13) {
			dn("Got line from: "); d(command);
			if (transferPage == (int) payload[1] &&
					(romManager.transferredData.data[(int) payload[2]] == 0 && 
					romManager.transferredData.data[((int) payload[2] + 1) % MAX_ROWS] == 0) &&
					(int) payload[2] == transferRow) {
				transferRow = ((int) payload[2] + 2) % MAX_ROWS;
				transferPageSuccess = (int) payload[2] == MAX_ROWS - 2;
				lastTransferTime = !transferPageSuccess ? millis() : lastTransferTime;
				dataTransferTime = !transferPageSuccess ? millis() : dataTransferTime;
				romManager.transferredData.data[(int) payload[2]] = (int) payload[3] * 100000000 + 
												(int) payload[4] * 1000000 + (int) payload[5] * 10000 + 
												(int) payload[6] * 100 + (int) payload[7] * 1;
				romManager.transferredData.data[((int) payload[2] + 1) % MAX_ROWS] = (int) payload[8] * 100000000 + 
												(int) payload[9] * 1000000 + (int) payload[10] * 10000 + 
												(int) payload[11] * 100 + (int) payload[12] * 1;
				Serial.println(String((int) payload[0]) + '\t' +
										String((int) payload[1]) + '\t' +
										String((int) payload[2]) + '\t' +
										(((romManager.transferredData.data[(int) payload[2]]) == (int) -1) ? "-1" : String(romManager.transferredData.data[(int) payload[2]])));
				Serial.println(String((int) payload[0]) + '\t' +
											 String((int) payload[1]) + '\t' +
											 String((((int) payload[2]) + 1) % MAX_ROWS) + '\t' +
											 (((romManager.transferredData.data[(((int) payload[2]) + 1) % MAX_ROWS]) == (int) -1) ? "-1" : String(romManager.transferredData.data[(((int) payload[2]) + 1) % MAX_ROWS])));
			}
		}
		*/
	}
}

void synchronizeTime() {
	if(!RTC_FLAG) {
		return;
	}
	RTC_FLAG = false;
	
	if (!timer.isTimeSet) {
		return;
	}
	
	DS3231_get(&rtcTime);
	timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
						rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);
	Serial.print("RTC Time: ");
	timer.displayDateTime();
	
	// TODO: Sync RTC time flag
	if (timer.isTimeSet) {
		char payload[] = {timer.t.month, timer.t.date, timer.t.year, timer.t.day,
							timer.t.hours, timer.t.minutes, timer.t.seconds};
		d("SimbleCOM send time");
		SimbleeCOM.send(payload, sizeof(payload));
	}
}