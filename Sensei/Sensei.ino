#include "Libraries\ds3231\ds3231.h"
#include "Libraries\PrNetRomManager\PrNetRomManager.h"
#include "Libraries\PrNetRomManager\util.h"
#include "Libraries\PrTime\PrTime.h"
#include <SimbleeCOM.h>
#include <Wire.h>

#define NETWORK_SIZE			16
#define MAXIMUM_NETWORK_SIZE	42
#define TX_POWER_LEVEL			-12
#define BAUD_RATE				250000
#define USE_SERIAL_MONITOR		true

#define MOTHER_NODE				true
#define REGION_TRACKER			false
#define LESSON_TRACKER			false
#define STUDENT_TRACKER			false

#if MOTHER_NODE == true
#define IS_MOTHER_NODE
#endif

#define START_HOUR				9
#define START_MINUTE			0
#define END_HOUR				16
#define END_MINUTE				0

#define MS_SEND_DELAY_MIN		5
#define MS_SEND_DELAY_MAX		50
#define SECONDS_TO_COLLECT		1
#define SECONDS_TO_TRACK		15
#define SECONDS_TO_ACK_TIME		60
#define SECONDS_TO_TRANSFER		5
#define MINUTES_BETWEEN_SYNC	20

#define RTC_INTERRUPT_PIN		10
#define UNUSED_ANALOG_PIN		2
#define ACCEL_POWER_PIN			23
#define X_PIN					4
#define Z_PIN					6

#define DS3231_ADDR				0x68
#define CTRL_REG				0x0E
#define START_INT				0b01000000
#define STOP_INT				0b00000100


#define ROW_RESET	(0)
#define ROW_TIME	(1)
#define ROW_ACCEL	(2)

#define DEBUG (1)

#ifdef DEBUG
#define d(x) Serial.println(x)
#define dn(x) Serial.print(x)
#else
#define d(x)
#define dn(x)
#endif

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Werror"

#define COMMAND_SHARED_TIME 254
#define COMMAND_RESPONSE_ROWS 255

// Time keeping data structure
Time timer;
// RTC time data structure
struct ts rtcTime;
// ROM Manager data structure
PrNetRomManager romManager;
// RSSI total and count for each device
int rssiTotal[NETWORK_SIZE];
int rssiCount[NETWORK_SIZE];

uint16_t deviceIndex[NETWORK_SIZE];
uint8_t devicesSeen = 0;

uint16_t getDeviceUid(uint32_t esn)
{
	return (esn & 0xFFFF) ^ ((esn >> 16) & 0xFFFF);
}

uint8_t getDeviceIndex(uint32_T esn)
{
	uint16_t uid = getDeviceUid(esn);
	for(uint8_t i = 0; i < NETWORK_SIZE; i++) {
		if(deviceIndex[i] == uid)
		{
			return i;
		}
	}
	deviceIndex[devicesSeen] = uid;
	if(devicesSeen >= NETWORK_SIZE) {
		d("Device array overflow - increase NETWORK_SIZE");
	}
	return devicesSeen++;
}



#ifdef IS_MOTHER_NODE
// Mother node device tracking
unsigned long deviceOnlineTime[NETWORK_SIZE];
#endif

// Boolean on whether received new data
bool newData;
// Boolean on whether to collect data
bool collectData;
// Boolean on whether radio stack is on
bool broadcasting;
// Time keeping and time sharing variables
int lastRTCSecond;
bool syncTime;
unsigned long ms;
// Time acknowledgment variable
unsigned long discoveryTime;
// Data transfer protocol state variables
bool transferROM;
int transferDevice;
int transferPage;
int transferRow;
bool transferPageSuccess;
bool lastPage;
bool erase;
unsigned long lastTransferTime;
unsigned long dataTransferTime;
// Accelerometer variables
bool accelerometerOn;
int xAccelerometerPrev;
int zAccelerometerPrev;
int xAccelerometerDiff;
int zAccelerometerDiff;
int accelerometerCount;

void setup() {
	pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
	Simblee_pinWakeCallback(RTC_INTERRUPT_PIN, HIGH, RTC_Interrupt);
	
	SimbleeCOM.txPowerLevel = MOTHER_NODE ? 4 : TX_POWER_LEVEL;
	
	randomSeed(analogRead(UNUSED_ANALOG_PIN));
	
	Serial.setTimeout(5);
	enableSerialMonitor();
	
	d("");
	if(MOTHER_NODE) {
		d("Mother node");
	}
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
		if(!MOTHER_NODE) {
			if (transferROM) {
				sendROMResponse();
			} else if (erase) {
				remoteEraseROM();
			}
		}
		delay(5);
		
		InterpretCommand();
		acknowledgeTimeReceipt();
	}
	if (!MOTHER_NODE && !USE_SERIAL_MONITOR) {
		Serial.end();
	}
}

void InterpretCommand()
{
	char ch;
	if(Serial.available() == 0)
		return;
	
	ch = Serial.read();
	d(ch);
	
	if (ch == 'T' || ch == 't') {
		programSystemTime();
	} else if (ch == 'I' || ch == 'i') {
		Serial.println(SimbleeCOM.getESN(), HEX);
	} else if (ch == 'P' || ch == 'p') {
		romManager.printROM();
	} else if (ch == 'E' || ch == 'e') {
		romManager.eraseROM();
	} else if ((ch == 'D' || ch == 'd') && MOTHER_NODE) {
		parseROMRequest();
	} else if ((ch == 'O' || ch == 'o') && MOTHER_NODE) {
		printOnlineDevices();
	} else if (ch == ' ') {
		// Ignore spaces
	} else {
		Serial.println("Available commands:");
		Serial.println("\tT: program system time");
		Serial.println("\tI: print device ESN (unique ID)");
		Serial.println("\tP: print ROM");
		Serial.println("\tE: erase ROM");
		if(MOTHER_NODE) {
			Serial.println("\tD: parse ROM request");
			Serial.println("\tO: print online devices");
		}
	}
}

void loop() {
	synchronizeTime();
	
#ifdef IS_MOTHER_NODE
	delay(5);
	InterpretCommand();
#else
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
#endif
}

/*
 * Outputs whether each device is online
 */

#ifdef IS_MOTHER_NODE
void printOnlineDevices() {
	Serial.print("O" + String(":"));
	for (int i = 0; i < NETWORK_SIZE; i++) {
		Serial.print(String(millis() - deviceOnlineTime[i] < SECONDS_TO_TRACK * 1000 && millis() > SECONDS_TO_TRACK * 1000) + ",");
	}
	Serial.println();
}
#else
void printOnlineDevices() {}
#endif

#ifdef IS_MOTHER_NODE
int RTC_Interrupt(uint32_t ulPin) {
	timer.totalSecondsElapsed++;
	timer.secondsElapsed++;
	
	dn("Time: "); timer.displayDateTime();
	syncTime = true;
	return 0;
}
#else
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
	
	syncTime = (timer.secondsElapsed >= MINUTES_BETWEEN_SYNC * 60 &&
					!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) ||
				!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
					!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE) ||
				collectData;
	return 0;
}
#endif

#ifdef IS_MOTHER_NODE
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
#else
void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi) {
	dn("Msg "); dn(len); dn(" "); dn(esn); dn("("); dn(rssi); dn(") "); for(int i = 0; i < len; i++) PrintHexByte(payload[i]); d("");
	
	uint8_t command = payload[0];
	
	// Over the air protocol:
	// Byte 0:
	// 0: 
	// 1-50: Is a proximity beacon
	// 255 down: Is a packet
	if(command == 0) {
		
	} else if(command < 50) {
		// Proximity beacon
		// 0xID
		dn("Device online: "); d(command);
		
		if(command == deviceID) {
			d("Data transfer");
			erase = ((int) payload[1] == (char) -1) && ((int) payload[2] == (char) -1);
			transferROM = ((int) payload[1] != (char) -1) && ((int) payload[2] != (char) -1);
			transferPage = transferROM ? (int) payload[1] : transferPage;
			transferRow = transferROM ? (int) payload[2] : transferRow;
			if (romManager.loadedPage != transferPage && 
				transferPage >= LAST_STORAGE_PAGE) {
				romManager.loadPage(transferPage);
			}
		} else {
			// Recording RSSI data
			d("RSSI");
			if(timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
				uint8_t index = getDeviceIndex(esn);
				rssiTotal[index] += rssi;
				rssiCount[index]++;
				newData = true;
			}
		}
	} else switch(command) {
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
#endif

////////// Time Functions //////////

/*
 * Sets system time sent from serial monitor
 * Format: 
 */
void programSystemTime() {
	d("Waiting for time");
	
	char systemTime[16];
	while (Serial.available() < 16)
	delay(10);
	Serial.readBytes(systemTime, 16);
	for(int i = 0; i<16; i++)
	Serial.print(systemTime[i], HEX);
	Serial.println();
	int initialMS = (systemTime[13] - '0') * 100 + (systemTime[14] - '0') * 10 + (systemTime[15] - '0');
	delay(1000 - initialMS);
	timer.setInitialTime((systemTime[0] - '0') * 10 + (systemTime[1] - '0'), // MM
						(systemTime[2] - '0') * 10 + (systemTime[3] - '0'), // DD
						(systemTime[4] - '0') * 10 + (systemTime[5] - '0'), // YY
						(systemTime[6] - '0'), // Day
						(systemTime[7] - '0') * 10 + (systemTime[8] - '0'), // HH
						(systemTime[9] - '0') * 10 + (systemTime[10] - '0'), // MM
						(systemTime[11] - '0') * 10 + (systemTime[12] - '0')); // SS
	timer.totalSecondsElapsed = 1;
	timer.secondsElapsed = 1;
	setRTCTime();
	startInterrupt();
	Serial.print("Time set to ");
	timer.displayDateTime();
}

/*
 * Synchonrizes the node's time with the RTC time and shares time
 */
void synchronizeTime() {
	if (syncTime) {
		DS3231_get(&rtcTime);
		timer.setInitialTime(rtcTime.mon, rtcTime.mday, rtcTime.year_s,
							rtcTime.wday, rtcTime.hour, rtcTime.min, rtcTime.sec);
		lastRTCSecond = rtcTime.sec;
		syncTime = false;
		Serial.print("RTC Time: ");
		timer.displayDateTime();
	}

	// Shares RTC time at rising edge of the clock second
	if (MOTHER_NODE ||
		collectData ||
		(!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
			!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE))) {
		DS3231_get(&rtcTime);
		if (rtcTime.sec == (lastRTCSecond + 1) % 60) {
			lastRTCSecond = rtcTime.sec;
			startBroadcast();
			if ((MOTHER_NODE && timer.totalSecondsElapsed > 5) ||
				(!MOTHER_NODE && timer.totalSecondsElapsed > 15)) {
				char payload[] = {timer.t.month, timer.t.date, timer.t.year, timer.t.day,
									timer.t.hours, timer.t.minutes, timer.t.seconds};
				d("SimbleCOM send time");
				SimbleeCOM.send(payload, sizeof(payload));
			} else {
				char payload[] = {};
				d("SimbleCOM send ID");
				SimbleeCOM.send(payload, sizeof(payload));
			}
		}
	}
}

/*
 * Acknowledges receipt of time for non-mother node sensors
 */
#ifdef IS_MOTHER_NODE
void acknowledgeTimeReceipt() {}
#else
void acknowledgeTimeReceipt() {
	discoveryTime = millis();
	while (!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
			!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
		delay(100);
		synchronizeTime();
	}
}
#endif

////////// Radio Functions //////////

/*
 * Enables radio broadcasting
 */
void startBroadcast() {
	if(broadcasting) {
		return;
	}
	
	SimbleeCOM.begin();
	broadcasting = true;
}

/*
 * Disables radio broadcasting
 */
void stopBroadcast() {
	if(!broadcasting) {
		return;
	}
	
	SimbleeCOM.end();
	broadcasting = false;
}

////////// ROM Functions //////////

/*
 * Stores deviceID (I), RSSI (R), and time (T)
 */
void writeData() {
	if (romManager.config.pageCounter < LAST_STORAGE_PAGE) {
		return;
	}
	if (!REGION_TRACKER && accelerometerCount > 0) {
		writeDataRow(ROW_ACCEL);
		clearAccelerometerState();
	}
	if (newData) {
		newData = false;
		timer.updateTime();
		// Time format: (total seconds since 5am)/10 (fits into 13 bits)
		int time = (((((timer.t.hours - 5) * 60) + timer.t.minutes) * 60 + timer.t.seconds) / 10);
		
		romManager.loadPage(romManager.config.pageCounter);
		
		for (int i = 0; i < devicesSeen; i++) {
			uint16_t deviceUid = deviceIndex[i];
			int rssiAverage = (rssiCount[i] == 0) ? -128 : rssiTotal[i] / rssiCount[i];
			Serial.println(String(i) + "\t" + String(deviceUid) + "\t" + String(i) + "\t" + String(rssiAverage) + "\t" + String(rssiCount[i]));
			if (rssiAverage > -100) {
				if (romManager.config.rowCounter >= MAX_ROWS) {
					romManager.writePage(romManager.config.pageCounter, romManager.table);
				}
				
				if(rssiAverage > -40) {
					rssiAverage = 3;
				} else if(rssiAverage > -60) {
					rssiAverage = 2;
				} else if(rssiAverage > -80) {
					rssiAverage = 1;
				} else { // > -100
					rssiAverage = 0;
				}
				
				// 0b1, Time (13 bits), rssi (2 bits), unique ID (16 bits)
				romManager.table.data[romManager.config.rowCounter] = 0x8000 | // Time row header
																		time & 0x1FFF << 18 |
																		rssiAverage & 0x3 << 16 |
																		deviceUid & 0xFFFF;
				romManager.config.rowCounter++;
			}
			rssiTotal[i] = 0;
			rssiCount[i] = 0;
		}
		
		// Erase everything after the current row (?)
		for (int i = romManager.config.rowCounter; i < MAX_ROWS; i++) {
			romManager.table.data[i] = -1;
		}
		romManager.writePage(romManager.config.pageCounter, romManager.table);
	}
}

/*
 * Stores timestamp OR
 * Stores x and z accelerometer data OR
 * Stores reset marker as 999,999,999
 */
void writeDataRow(uint8_t data) {
	if (romManager.config.pageCounter < LAST_STORAGE_PAGE) {
		return;
	}
	if (romManager.config.rowCounter >= MAX_ROWS) {
		romManager.writePage(romManager.config.pageCounter, romManager.table);
	}
	romManager.loadPage(romManager.config.pageCounter);
	if (data == ROW_TIME) {
		timer.updateTime();
		// 0b00 14 bits 0x0000... time
		int time = ((((timer.t.hours * 60) + timer.t.minutes) * 60 + timer.t.seconds) / 10);
		romManager.table.data[romManager.config.rowCounter] = time;
	} else if (data == ROW_ACCEL) {
		// 28 bits
		// 0b01 XXXXXXXXXXXXXXZZZZZZZZZZZZZZ
		romManager.table.data[romManager.config.rowCounter] = 0x4000 | // Accel row header
																(min(xAccelerometerDiff / (10 * accelerometerCount), 0x3FFF)) |
																(min(zAccelerometerDiff / (10 * accelerometerCount), 0x3FFF) << 14);
	} else if (data == ROW_RESET) {
		// 0b0011 (0x3B9AC9FF)
		romManager.table.data[romManager.config.rowCounter] = 999999999;
	}
	romManager.config.rowCounter++;
	romManager.writePage(romManager.config.pageCounter, romManager.table);
}

/*
 * Parses ROM request from the cellphone application
 */
void parseROMRequest() {
	Serial.println("Starting Data Transfer");
	int input = 0;
	while(input = Serial.read() != '.') {
		transferDevice = input - '0';
		sendROMRequest();
	}
	Serial.println("Data Transfer Complete");
}

/*
 * Send request for ROM data to network nodes
 */
void sendROMRequest() {
	transferPage = STORAGE_FLASH_PAGE;
	transferROM = true;
	bool timedOut = false;
	dataTransferTime = millis();
	while (transferROM) {
		transferPageSuccess = lastPage = false;
		while (!transferPageSuccess) {
			if (timer.timeout(&lastTransferTime, 50)) {
				char payload[] = {transferDevice, transferPage, transferRow};
				SimbleeCOM.send(payload, sizeof(payload));
				lastTransferTime = millis();
			}
			delay(50);
			if (timer.timeout(&dataTransferTime, SECONDS_TO_TRANSFER * 1000)) {
				dataTransferTime = millis();
				transferROM = false;
				timedOut = true;
				break;
			}
		}
		romManager.clearTransferredData();
		if (lastPage || transferPage <= LAST_STORAGE_PAGE) {
			while (transferROM) {
				delay(50);
				char payload[] = {transferDevice, -1, -1};
				SimbleeCOM.send(payload, sizeof(payload));
			}
		}
		transferPage--;
	}
	Serial.println("D," + String(transferDevice) + "," + String(timedOut));
}

/*
 * Send response for ROM data to mother node
 */
void sendROMResponse() {
	Serial.println("Transferring Page " + String(transferPage));
	data *p = (data*) ADDRESS_OF_PAGE(transferPage);
	lastPage = (p -> data[0]) == -1 || transferPage <= LAST_STORAGE_PAGE;
	if (lastPage) {
		delay(15);
		char payload[] = {transferPage};
		SimbleeCOM.send(payload, sizeof(payload));
	}
	while (transferRow < MAX_ROWS) {
		delay(15);
		char payload[11];
		payload[0] = COMMAND_RESPONSE_ROWS;
		payload[1] = transferPage;
		payload[2] = transferRow;
		for(int i = 0; i < 4; i++){
			payload[3+i] = (romManager.table.data[transferRow] >> (8*i))& 0xFF;
			payload[7+i] = (romManager.table.data[(transferRow + 1) % MAX_ROWS] >> (8*i))& 0xFF;
		}
		
		d("");
		d("---");
		PrintHexInt(romManager.table.data[transferRow]); d("");
		PrintHexInt(romManager.table.data[(transferRow + 1) % MAX_ROWS]); d("");
		d("");
		for(int i = 0; i < 11; i++){
			PrintHexByte(payload[i]);
		}
		SimbleeCOM.send(payload, sizeof(payload));
		transferRow += 2;
	}
	transferROM = false;
	Serial.println("Transferred Page " + String(transferPage));
}

/*
 * Remotely erases sensors after successful data transfer to mother node
 */
void remoteEraseROM() {
	Serial.println("Erasing ROM");
	data *p = (data*) ADDRESS_OF_PAGE(STORAGE_FLASH_PAGE);
	if ((p -> data[0]) != -1) {
		romManager.eraseROM();
		romManager.resetConfig();
	}
	erase = false;
	char payload[] = {deviceID, -1, -1};
	SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Erases ROM and resets ROM configuration if sensor is reprogrammed or write ROM reset otherwise
 */
void resetROM() {
	(romManager.config.pageCounter == -1) ? romManager.resetConfig() : writeDataRow(ROW_RESET);
}

////////// RTC Functions //////////

/*
 * Starts the RTC clock
 */
void setRTCTime() {
	timer.updateTime();
	rtcTime.mon = timer.t.month;
	rtcTime.mday = timer.t.date;
	rtcTime.year = 2000 + timer.t.year;
	rtcTime.wday = timer.t.day;
	rtcTime.hour = timer.t.hours;
	rtcTime.min = timer.t.minutes;
	rtcTime.sec = timer.t.seconds;
	DS3231_set(rtcTime);
}

/*
 * Starts the RTC interrupt
 */
void startInterrupt() {
	Wire.beginOnPins(14, 13);
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(CTRL_REG);
	Wire.write(START_INT);
	Wire.endTransmission();
}

/*
 * Stops the RTC interrupt
 */
void stopInterrupt() {
	Wire.beginOnPins(14, 13);
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(CTRL_REG);
	Wire.write(STOP_INT);
	Wire.endTransmission();
}

////////// Accelerometer Functions //////////

void setupAccelerometer() {
	pinMode(ACCEL_POWER_PIN, OUTPUT);
}

void enableAccelerometer() {
	digitalWrite(ACCEL_POWER_PIN, HIGH);
}

void disableAccelerometer() {
	digitalWrite(ACCEL_POWER_PIN, LOW);
}

void readAcc(){
	int xAccelerometerCurr = analogRead(X_PIN);
	int zAccelerometerCurr = analogRead(Z_PIN);
	xAccelerometerDiff += (xAccelerometerCurr - xAccelerometerPrev) * (xAccelerometerCurr - xAccelerometerPrev);
	zAccelerometerDiff += (zAccelerometerCurr - zAccelerometerPrev) * (zAccelerometerCurr - zAccelerometerPrev);
	xAccelerometerPrev = xAccelerometerCurr;
	zAccelerometerPrev = zAccelerometerCurr;
	accelerometerCount++;
}

void clearAccelerometerState() {
	xAccelerometerDiff = 0;
	zAccelerometerDiff = 0;
	xAccelerometerPrev = 0;
	zAccelerometerPrev = 0;
	accelerometerCount = 0;
}

////////// Serial Monitor Functions //////////

/*
 * Enables the serial monitor
 */
void enableSerialMonitor() {
	if (USE_SERIAL_MONITOR) {
		Serial.begin(BAUD_RATE);
	}
}

/*
 * Disables the serial monitor
 */
void disableSerialMonitor() {
	if (USE_SERIAL_MONITOR) {
		Serial.end();
	}
}

