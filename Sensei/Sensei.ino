#include "Libraries\ds3231\ds3231.h"
#include "Libraries\PrNetRomManager\PrNetRomManager.h"
#include "Libraries\PrTime\PrTime.h"
#include <SimbleeCOM.h>
#include <Wire.h>

#define NETWORK_SIZE					16
#define MAXIMUM_NETWORK_SIZE	42
#define TX_POWER_LEVEL				-12
#define BAUD_RATE						 115200
#define USE_SERIAL_MONITOR		false

#define MOTHER_NODE					 true
#define REGION_TRACKER				false
#define LESSON_TRACKER				false
#define BOARDS								false

#define START_HOUR						9
#define START_MINUTE					0
#define END_HOUR							16
#define END_MINUTE						0

#define MS_SEND_DELAY_MIN		 5
#define MS_SEND_DELAY_MAX		 50
#define SECONDS_TO_COLLECT		1
#define SECONDS_TO_TRACK			15
#define SECONDS_TO_ACK_TIME	 60
#define SECONDS_TO_TRANSFER	 5
#define MINUTES_BETWEEN_SYNC	20

#define INTERRUPT_PIN				 BOARDS ? 10 : 2
#define UNUSED_PIN						BOARDS ? 2 : 3
#define DS3231_ADDR					 0x68
#define CTRL_REG							0x0E
#define START_INT						 0b01000000
#define STOP_INT							0b00000100

#define POWER_PIN						23
#define X_PIN								4
#define Z_PIN								6

#define DEBUG

#ifdef DEBUG
#define d(x) Serial.println(x);
#else
#define d(x)
#endif

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Werror"

// Unique device ID
int deviceID = 0;
// Time keeping data structure
Time timer;
// RTC time data structure
struct ts rtcTime;
// ROM Manager data structure
PrNetRomManager romManager;
// RSSI total and count for each device
int rssiTotal[NETWORK_SIZE];
int rssiCount[NETWORK_SIZE];
// Mother node device tracking
unsigned long deviceOnlineTime[NETWORK_SIZE];
// Boolean on whether received new data
bool newData;
// Boolean on whether to collect data
bool collectData;
// Boolean on whether radio stack is on
bool broadcasting;
// Time keeping and time sharing variables
int interruptTime;
int lastRTCSecond;
bool syncTime;
unsigned long ms;
bool foundRTCTransition;
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
	pinMode(INTERRUPT_PIN, INPUT_PULLUP);
	Simblee_pinWakeCallback(INTERRUPT_PIN, HIGH, onInterrupt);
	randomSeed(analogRead(UNUSED_PIN));
	Serial.setTimeout(5);
	setupSensor();
	setupAccelerometer();
	enableSerialMonitor();
	SimbleeCOM.txPowerLevel = MOTHER_NODE ? 4 : TX_POWER_LEVEL;
	
	if(MOTHER_NODE) {
		Serial.println("Mother node");
	}
	if(REGION_TRACKER) {
		Serial.println("Region tracker");
	}
	if(LESSON_TRACKER) {
		Serial.println("Lesson tracker");
	}
	if(BOARDS) {
		Serial.println("Student sensor");
	}
}

void loop() {
	synchronizeTime();
	shareTime();
	if (MOTHER_NODE) {
		delay(100);
		printOnlineDevices();
	} else {
		if (collectData && foundRTCTransition) {
			startBroadcast();
			delay(random(MS_SEND_DELAY_MIN, MS_SEND_DELAY_MAX));
			if (!REGION_TRACKER) {
				enableAccelerometer();
				readAcc();
				timer.updateTime();
				char payload[] = {deviceID};
				SimbleeCOM.send(payload, sizeof(payload));
			}
		} else {
			stopBroadcast();
			disableAccelerometer();
			writeData();
			Simblee_ULPDelay(INFINITE);
		}
	}
}

/*
 * Outputs whether each device is online
 */
void printOnlineDevices() {
	Serial.print("O" + String(","));
	for (int i = 0; i < NETWORK_SIZE; i++) {
		Serial.print(String(millis() - deviceOnlineTime[i] < SECONDS_TO_TRACK * 1000 && millis() > SECONDS_TO_TRACK * 1000) + ",");
	}
	Serial.println();
}

int onInterrupt(uint32_t ulPin) {
	interruptTime += millis() - ms;
	if (interruptTime > 995) {
		timer.totalSecondsElapsed++;
		timer.secondsElapsed++;
		interruptTime = 0;
	}
	foundRTCTransition = false;
	timer.displayDateTime();
	collectData = ((timer.initialTime.seconds + timer.secondsElapsed) % 10 < SECONDS_TO_COLLECT) && timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE);
	syncTime = (timer.secondsElapsed >= MINUTES_BETWEEN_SYNC * 60 && !timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) ||
						 !timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) && !timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE) ||
						 collectData ||
						 MOTHER_NODE;
	return collectData || syncTime;
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi) {
	d(payload);
	
	// Recording RSSI data
	if ((int) payload[0] >= 0 && (int) payload[0] < NETWORK_SIZE && (len == 1 || len == 8) &&
		!MOTHER_NODE && timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
		rssiTotal[(int) payload[0]] += rssi;
		rssiCount[(int) payload[0]]++;
		newData = true;
	// Mother node online device tracking
	} else if ((int) payload[0] >= 0 && (int) payload[0] < NETWORK_SIZE && (len == 1 || len == 8) && MOTHER_NODE) {
		deviceOnlineTime[(int) payload[0]] = millis();
	// Time sharing
	} else if (!MOTHER_NODE && !timer.isTimeSet && len == 8) {
		timer.setInitialTime((int) payload[1], (int) payload[2], (int) payload[3], (int) payload[4],
			(int) payload[5], (int) payload[6], (int) payload[7]);
		timer.totalSecondsElapsed = 1;
		timer.secondsElapsed = 1;
		interruptTime = 0;
		setRTCTime();
		startInterrupt();
	// Data transfer protocol
	} else if ((int) payload[0] == deviceID && len == 3 && !MOTHER_NODE) {
		erase = ((int) payload[1] == (char) -1) && ((int) payload[2] == (char) -1);
		transferROM = ((int) payload[1] != (char) -1) && ((int) payload[2] != (char) -1);
		transferPage = transferROM ? (int) payload[1] : transferPage;
		transferRow = transferROM ? (int) payload[2] : transferRow;
		if (romManager.loadedPage != transferPage && transferPage >= LAST_STORAGE_PAGE) {
			romManager.loadPage(transferPage);
		}
	} else if ((int) payload[0] >= 0 && (int) payload[0] < NETWORK_SIZE && transferROM && len == 13 && MOTHER_NODE) {
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
	} else if (len == 2 && MOTHER_NODE) {
		lastPage = true;
		lastTransferTime = millis();
	} else if ((int) payload[0] == transferDevice && len == 3 && MOTHER_NODE) {
		transferROM = (((int) payload[1] != (char) -1) || ((int) payload[2] != (char) -1));
	}
}

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
	interruptTime = 0;
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
		Serial.print("Synced Time: ");
		timer.displayDateTime();
	}
}

/*
 * Shares RTC time at rising edge of the clock second
 */
void shareTime() {
	if (MOTHER_NODE || collectData || (!timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
		!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE))) {
		DS3231_get(&rtcTime);
		if (rtcTime.sec == (lastRTCSecond + 1) % 60) {
			lastRTCSecond = rtcTime.sec;
			foundRTCTransition = true;
			startBroadcast();
			if ((MOTHER_NODE && timer.totalSecondsElapsed > 5) || (!MOTHER_NODE && timer.totalSecondsElapsed > 15)) {
				char payload[] = {deviceID, timer.t.month, timer.t.date, timer.t.year, timer.t.day,
									timer.t.hours, timer.t.minutes, timer.t.seconds};
				SimbleeCOM.send(payload, sizeof(payload));
			} else {
				char payload[] = {deviceID};
				SimbleeCOM.send(payload, sizeof(payload));
			}
		}
	}
}

/*
 * Acknowledges receipt of time for non-mother node sensors
 */
void acknowledgeTimeReceipt() {
	discoveryTime = millis();
	while (!MOTHER_NODE && !timer.timeout(&discoveryTime, SECONDS_TO_ACK_TIME * 1000) &&
			!timer.inDataCollectionPeriod(START_HOUR, START_MINUTE, END_HOUR, END_MINUTE)) {
		delay(100);
		synchronizeTime();
		shareTime();
	}
}

////////// Radio Functions //////////

/*
 * Enables radio broadcasting
 */
void startBroadcast() {
	if (!broadcasting) {
		SimbleeCOM.begin();
		broadcasting = true;
	}
}

/*
 * Disables radio broadcasting
 */
void stopBroadcast() {
	if (broadcasting) {
		SimbleeCOM.end();
		broadcasting = false;
	}
}

////////// ROM Functions //////////

/*
 * Stores deviceID (I), RSSI (R), and time (T) as I,IRR,TTT,TTT with maximum value of 4,294,967,295, (2^32 - 1)
 * Stores x and z accelerometer data as 4,1ZZ,ZZX,XXX
 */
void writeData() {
	if (romManager.config.pageCounter < LAST_STORAGE_PAGE) {
		return;
	}
	if (!REGION_TRACKER && accelerometerCount > 0) {
		writeDataRow("Accelerometer");
		clearAccelerometerState();
	}
	if (newData) {
		timer.updateTime();
		int data = (timer.t.seconds % 60) + ((timer.t.minutes % 60) * 100) + ((timer.t.hours % 24) * 10000);
		romManager.loadPage(romManager.config.pageCounter);
		for (int i = 0; i < NETWORK_SIZE; i++) {
			int rssiAverage = (rssiCount[i] == 0) ? -128 : rssiTotal[i] / rssiCount[i];
			Serial.println(String(i) + "\t" + String(rssiAverage) + "\t" + String(rssiCount[i]));
			if (rssiAverage > -100) {
				if (romManager.config.rowCounter >= MAX_ROWS) {
					romManager.writePage(romManager.config.pageCounter, romManager.table);
				}
				romManager.table.data[romManager.config.rowCounter] = data + (abs(rssiAverage % 100) * 1000000) +
																		((i % MAXIMUM_NETWORK_SIZE) * 100000000);
				romManager.config.rowCounter++;
			}
			rssiTotal[i] = 0;
			rssiCount[i] = 0;
		}
		for (int i = romManager.config.rowCounter; i < MAX_ROWS; i++) {
			romManager.table.data[i] = -1;
		}
		romManager.writePage(romManager.config.pageCounter, romManager.table);
	}
	newData = false;
}

/*
 * Stores timestamp as HHM,MSS
 * Stores x and z accelerometer data as 4,1ZZ,ZZX,XXX
 * Stores reset marker as 999,999,999
 */
void writeDataRow(String data) {
	if (romManager.config.pageCounter < LAST_STORAGE_PAGE) {
		return;
	}
	if (romManager.config.rowCounter >= MAX_ROWS) {
		romManager.writePage(romManager.config.pageCounter, romManager.table);
	}
	romManager.loadPage(romManager.config.pageCounter);
	if (data == "Time") {
		timer.updateTime();
		romManager.table.data[romManager.config.rowCounter] = (timer.t.seconds % 60) +
																((timer.t.minutes % 60) * 100) +
																((timer.t.hours % 24) * 10000);
	} else if (data == "Accelerometer") {
		romManager.table.data[romManager.config.rowCounter] = (min(xAccelerometerDiff / (10 * accelerometerCount), 9999) * 1) + 
										(min(zAccelerometerDiff / (10 * accelerometerCount), 9999) * 10000) + 4100000000;
	} else if (data == "Reset") {
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
	String input = Serial.readString();
	for (int i = 0; i < NETWORK_SIZE; i++) {
		if (input.charAt(i) - '0' && input.charAt(i) != '\0') {
			transferDevice = i;
			sendROMRequest();
		}
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
		char payload[] = {deviceID, transferPage};
		SimbleeCOM.send(payload, sizeof(payload));
	}
	while (transferRow < MAX_ROWS) {
		delay(15);
		char payload[] = {deviceID, transferPage, transferRow,
			(romManager.table.data[transferRow] - (romManager.table.data[transferRow] / 10000000000) * 10000000000) / 100000000,
			(romManager.table.data[transferRow] - (romManager.table.data[transferRow] / 100000000) * 100000000) / 1000000,
			(romManager.table.data[transferRow] - (romManager.table.data[transferRow] / 1000000) * 1000000) / 10000,
			(romManager.table.data[transferRow] - (romManager.table.data[transferRow] / 10000) * 10000) / 100,
			(romManager.table.data[transferRow] - (romManager.table.data[transferRow] / 100) * 100) / 1,
			(romManager.table.data[(transferRow + 1) % MAX_ROWS] - (romManager.table.data[(transferRow + 1) % MAX_ROWS] / 10000000000) * 10000000000) / 100000000,
			(romManager.table.data[(transferRow + 1) % MAX_ROWS] - (romManager.table.data[(transferRow + 1) % MAX_ROWS] / 100000000) * 100000000) / 1000000,
			(romManager.table.data[(transferRow + 1) % MAX_ROWS] - (romManager.table.data[(transferRow + 1) % MAX_ROWS] / 1000000) * 1000000) / 10000,
			(romManager.table.data[(transferRow + 1) % MAX_ROWS] - (romManager.table.data[(transferRow + 1) % MAX_ROWS] / 10000) * 10000) / 100,
			(romManager.table.data[(transferRow + 1) % MAX_ROWS] - (romManager.table.data[(transferRow + 1) % MAX_ROWS] / 100) * 100) / 1};
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
	(romManager.config.pageCounter == -1) ? romManager.resetConfig() : writeDataRow("Reset");
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
	BOARDS ? Wire.beginOnPins(14, 13) : Wire.begin();
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(CTRL_REG);
	Wire.write(START_INT);
	Wire.endTransmission();
}

/*
 * Stops the RTC interrupt
 */
void stopInterrupt() {
	BOARDS ? Wire.beginOnPins(14, 13) : Wire.begin();
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(CTRL_REG);
	Wire.write(STOP_INT);
	Wire.endTransmission();
}

////////// Accelerometer Functions //////////

void setupAccelerometer() {
	if (!REGION_TRACKER || !MOTHER_NODE) {
		pinMode(POWER_PIN, OUTPUT);
	}
}

void enableAccelerometer() {
	if (!accelerometerOn) {
		digitalWrite(POWER_PIN, HIGH);
	}
}

void disableAccelerometer() {
	if (accelerometerOn) {
		digitalWrite(POWER_PIN, LOW);
	}
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

////////// Sensor Setup Functions //////////

/*
 * Interfaces with device to receive time or print ROM data
 */
void setupSensor() {
	Serial.begin(BAUD_RATE);
	resetROM();
	stopInterrupt();
	startBroadcast();
	char ch;
	while (!timer.isTimeSet) {
		Serial.println("> ");
		while(Serial.available() == 0)
		{
			if (transferROM && !MOTHER_NODE) {
				sendROMResponse();
			} else if (erase && !MOTHER_NODE) {
				remoteEraseROM();
			}
			delay(5);
		}
		ch = Serial.read();
		if (ch == 'T' || ch == 't') {
			programSystemTime();
		} else if (ch == 'P' || ch == 'p') {
			romManager.printROM();
		} else if (ch == 'E' || ch == 'e') {
			romManager.eraseROM();
		} else if ((ch == 'D' || ch == 'd') && MOTHER_NODE) {
			parseROMRequest();
		} else {
			Serial.println("Available commands:")
			Serial.println("\tT: program system time");
			Serial.println("\tP: print ROM");
			Serial.println("\tE: erase ROM");
			if(MOTHER_NODE) {
				Serial.println("\tD: parse ROM request");
			}
		}
	}
	acknowledgeTimeReceipt();
	if (!MOTHER_NODE && !USE_SERIAL_MONITOR) {
		Serial.end();
	}
}
