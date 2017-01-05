#include <Arduino.h>
#include <SimbleeCOM.h>
#include "debug.h"
#include "config.h"
#include "rtc.h"
#include "rom.h"
#include "radio_common.h"

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
		Serial.println(getDeviceUid(SimbleeCOM.getESN()), HEX);
	} else if (ch == 'P' || ch == 'p') {
		romManager.printROM();
	} else if (ch == 'E' || ch == 'e') {
		romManager.eraseROM();
	} else
		#ifdef MOTHER_NODE
		if (ch == 'D' || ch == 'd') {
			// Request all pages
			parseROMRequest();
		} else if(ch == 'S' || ch == 's') {
			// Request single page
			parseROMRequest();
		} else if (ch == 'L' || ch == 'l') {
			// Tell sensor device to go to sleep
			
		} else if (ch == 'Z' || ch == 'z') {
			// Program device ID
			
		} else if (ch == 'O' || ch == 'o') {
			// Print list of online devices
			printOnlineDevices();
		} else 
		#endif
	if (ch == ' ') {
		// Ignore spaces
	} else {
		Serial.println("Available commands:");
		Serial.println("\tT: program system time");
		Serial.println("\tI: print device ID");
		Serial.println("\tP: print ROM");
		Serial.println("\tE: erase ROM");
		#ifdef MOTHER_NODE
			Serial.println("\tD: sensor device ROM request");
			Serial.println("\tO: print online devices");
		#endif
	}
}

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
