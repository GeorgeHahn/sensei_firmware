#include <SimbleeCOM.h>
#include "rom.h"
#include "rtc.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// ROM Manager data structure
PrNetRomManager romManager;

// Data transfer protocol state variables
bool transferROM;
int transferPage;
int transferRow;
bool transferPageSuccess;
bool lastPage;

unsigned long lastTransferTime;
unsigned long dataTransferTime;

/*
 * Time format: (total seconds since 5am)/10 (fits into 13 bits)
 */
uint16_t GetTime() {
	// Fetch latest time from the timer
	timer.updateTime();

	return (((((timer.t.hours - 5) * 60) + timer.t.minutes) * 60 + timer.t.seconds) / 10) && 0x1FFF;
}

/*
 * Takes an accel reading if appropriate, then dumps prox events to flash
 */
void writeData() {
	// Check for room in flash
	if (romManager.config.pageCounter < LAST_STORAGE_PAGE) {
		return;
	}
	
	#if REGION_TRACKER == false
	if (accelerometerCount > 0) {
		writeDataRow(ROW_ACCEL);
		clearAccelerometerState();
	}
	#endif

	if (newData) {
		newData = false;
		writeDataRow(ROW_PROX);
	}
}

/*
 * Stores timestamp OR
 * Stores x and z accelerometer data OR
 * Stores proximity events with deviceID (I), RSSI (R), and time (T) OR
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
	if (data == ROW_PROX) {
		// Proximity event
		for (int i = 0; i < NETWORK_SIZE; i++) {
			int rssiAverage = (rssiCount[i] == 0) ? -128 : rssiTotal[i] / rssiCount[i];
			Serial.println(String(i) + "\t" + String(i) + "\t" + String(i) + "\t" + String(rssiAverage) + "\t" + String(rssiCount[i]));

			// If the average RSSI was strong enough, write a proximity event row
			if (rssiAverage > -100) {
				if (romManager.config.rowCounter >= MAX_ROWS) {
					romManager.writePage(romManager.config.pageCounter, romManager.table);
				}
				
				// Convert rssiAverage to positive
				rssiAverage = -rssiAverage;
				
				// Write a row for this proximity event
				// 0b1, Time (13 bits), rssi (7 bits), unused (3 bits), ID (8 bits)
				// 0b1TTTTTTTTTTTTTRRRRRRRUUUIIIIIIII
				romManager.table.data[romManager.config.rowCounter] = DATA_ROW_HEADER | // 0b1...
																	GetTime() & 0x1FFF << 18 | // Time mask: 0x7FFC0000
																	rssiAverage & 0x7F << 11 | // RSSI mask: 0x0003F800
																	// Bits 8, 9, 10 are free
																	i & 0xFF; // Device ID mask: 0x000000FF
				romManager.config.rowCounter++;
			}

			// Reset average for this device
			rssiTotal[i] = 0;
			rssiCount[i] = 0;
		}
		
		// Erase everything after the current row (?)
		// for (int i = romManager.config.rowCounter; i < MAX_ROWS; i++) {
		// 	romManager.table.data[i] = -1;
		// }
		romManager.writePage(romManager.config.pageCounter, romManager.table);
		return;
	}
	
	if (data == ROW_TIME) {
		timer.updateTime();
		romManager.table.data[romManager.config.rowCounter] = TIME_ROW_HEADER | 
																GetTime();
	} else if (data == ROW_ACCEL) {
		// 30 bits
		// ZZZZZZZZZZZZZZZXXXXXXXXXXXXXXX
		romManager.table.data[romManager.config.rowCounter] = ACCEL_ROW_HEADER |
										(min(xAccelerometerDiff / (10 * accelerometerCount), 0x7FFF)) |
										(min(zAccelerometerDiff / (10 * accelerometerCount), 0x7FFF) << 15);
	} else if (data == ROW_RESET) {
		romManager.table.data[romManager.config.rowCounter] = RESET_ROW_HEADER;
	}
	romManager.config.rowCounter++;
	romManager.writePage(romManager.config.pageCounter, romManager.table);
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
		payload[0] = RADIO_RESPONSE_ROWS;
		payload[1] = transferPage;
		payload[2] = transferRow;
		for(int i = 0; i < 4; i++){
			payload[3+i] = (romManager.table.data[transferRow] >> (8*i)) & 0xFF;
			payload[7+i] = (romManager.table.data[(transferRow + 1) % MAX_ROWS] >> (8*i)) & 0xFF;
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
	char payload[] = {0, -1, -1}; // TODO deviceID
	SimbleeCOM.send(payload, sizeof(payload));
}

/*
 * Erases ROM and resets ROM configuration if sensor is reprogrammed or write ROM reset otherwise
 */
void resetROM() {
	(romManager.config.pageCounter == -1) ? romManager.resetConfig() : writeDataRow(ROW_RESET);
}
