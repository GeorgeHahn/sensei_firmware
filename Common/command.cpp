#include <Arduino.h>
#include <SimbleeCOM.h>
#include "command.h"
#include "debug.h"
#include "config.h"
#include "rtc.h"
#include "rom.h"
#include "radio_common.h"

// This reaches back up into the parent code to
// get board type
#include "../../config.h"

// Override schedule
bool alwaysCollectMode = false;

void InterpretCommand()
{
    char ch;

    // Don't block
    if (Serial.available() == 0)
        return;

    ch = Serial.read();
    d(ch);

    if (ch == 'T' || ch == 't') {
        programSystemTime();
    } else

#ifndef MOTHER_NODE
    if (ch == 'A' || ch == 'a') {
        alwaysCollectMode = ReadChar() == '1';
        if (alwaysCollectMode) {
            d("alwaysCollectMode is on");
        } else {
            d("alwaysCollectMode is off");
        }
    } else if (ch == 'I' || ch == 'i') {
        PrintByte(romManager.config.deviceID);
    } else if (ch == 'P' || ch == 'p') {
        romManager.printROM();
    } else if (ch == 'E' || ch == 'e') {
        romManager.eraseROM();
    } else if (ch == 'Z' || ch == 'z') {
        // Program device ID
        romManager.SetDeviceID(ReadHexByte());
    } else
#endif

// TODO: Add page usage info request (RADIO_REQUEST_PAGEINFO)
#ifdef MOTHER_NODE
    if (ch == 'D' || ch == 'd') {
        // Request full flash dump
        uint8_t id = ReadHexByte();
        RequestROMFull(id);
    } else if (ch == 'S' || ch == 's') {
        // Request a partial ROM dump
        //RequestROMPartial();
    } else if (ch == 'L' || ch == 'l') {
        // Tell sensor device to go to sleep
        uint8_t id = ReadHexByte();
        RequestSleepErase(id);
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
#ifndef MOTHER_NODE
        Serial.println("\tA1: Always collect data");
        Serial.println("\tA0: Collect data during scheduled times");
        Serial.println("\tI: print device ID");
        Serial.println("\tP: print ROM");
        Serial.println("\tE: erase ROM");
        Serial.println("\tZ: set device ID");
#endif
#ifdef MOTHER_NODE
        Serial.println("\tD: sensor device ROM request");
        Serial.println("\tO: print online devices");
#endif
    }
}
