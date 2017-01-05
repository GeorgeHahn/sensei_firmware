#include <Arduino.h>
#include <SimbleeCOM.h>
#include "rom.h"
#include "rtc.h"
#include "accel.h"
#include "radio_common.h"
#include "debug.h"

// RSSI total and count for each device
int rssiTotal[NETWORK_SIZE];
int rssiCount[NETWORK_SIZE];

// Boolean on whether radio stack is on
bool broadcasting;

// Boolean on whether received new data
bool newData;
// Boolean on whether to collect data
bool collectData;
// Time acknowledgment variable
unsigned long discoveryTime;

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