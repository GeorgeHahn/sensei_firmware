/*
  PrNetRomManager.h - Library for managing data in the flash rom of PrNet nodes.
  Created by Dwyane George, March 12, 2016, Adapted version from Nazmus Saquib,
  October 7, 2015
  Social Computing Group, MIT Media Lab
*/

#include "Arduino.h"
#include "util.h"
#include "PrNetRomManager.h"

PrNetRomManager::PrNetRomManager() { loadConfig(); }

bool PrNetRomManager::OutOfSpace()
{
    return config.pageCounter < LAST_STORAGE_PAGE;
}

void PrNetRomManager::CheckPageSpace()
{
    if (config.rowCounter >= MAX_ROWS) {
        writePage(config.pageCounter, table);
        config.pageCounter--;
        loadPage(config.pageCounter);
        config.rowCounter = 0;
    }
}

void PrNetRomManager::printPage(int page)
{
    data *p = (data *)ADDRESS_OF_PAGE(page);
    for (int i = 0; i < MAX_ROWS; i++) {
        if ((p->data[i]) == 0xFFFFFFFF) {
            // Don't print empty rows
        } else {
            PrintHexByte(page);
            PrintHexByte(i);
            Serial.print(": ");
            PrintHexInt(p->data[i]);
            Serial.println();
        }
    }
}

void PrNetRomManager::printROM()
{
    for (int i = STORAGE_FLASH_PAGE; i >= LAST_STORAGE_PAGE; i--) {
        printPage(i);
    }
    Serial.println("Printed ROM");
}

void PrNetRomManager::loadPage(int page)
{
    data *p = (data *)ADDRESS_OF_PAGE(page);
    for (int i = 0; i < MAX_ROWS; i++) {
        table.data[i] = p->data[i];
    }
    loadedPage = page;
}

int PrNetRomManager::erasePage(int page) { return flashPageErase(page); }

/*
 * Erases all flash data
 * TODO: Can we just erase from pageCounter up to STORAGE_FLASH_PAGE? (Only erase used pages)
 * TODO: Other option: Just reset pageCounter to 0 and erase pages before writing
 */
void PrNetRomManager::eraseROM()
{
    for (int i = STORAGE_FLASH_PAGE; i >= LAST_STORAGE_PAGE; i--) {
        erasePage(i);
    }
    config.pageCounter = STORAGE_FLASH_PAGE;
    config.rowCounter = 0;
    updateConfig();
    Serial.println("Erased ROM");
}

int PrNetRomManager::writePage(int page, struct data values)
{
    if (page >= LAST_STORAGE_PAGE) {
        data *p = (data *)ADDRESS_OF_PAGE(page);
        int rc = flashWriteBlock(p, &values, sizeof(values));
        if (config.rowCounter >= MAX_ROWS) {
            config.pageCounter--;
            config.rowCounter = 0;
        }
        updateConfig();
        return rc;
    } else {
        return -1;
    }
}

void PrNetRomManager::loadConfig()
{
    prnetConfig *p = (prnetConfig *)ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
    config.pageCounter = p->pageCounter;
    config.rowCounter = p->rowCounter;
    config.deviceID = p->deviceID;
}

// Save current settings to flash
int PrNetRomManager::updateConfig()
{
    erasePage(SETTINGS_FLASH_PAGE);
    prnetConfig *p = (prnetConfig *)ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
    return flashWriteBlock(p, &config, sizeof(config));
}

void PrNetRomManager::SetDeviceID(uint8_t newID)
{
    config.deviceID = newID;
    updateConfig();
}

void PrNetRomManager::resetConfig()
{
    config.pageCounter = STORAGE_FLASH_PAGE;
    config.rowCounter = 0;
    config.deviceID = 0;
    updateConfig();
    Serial.println("Reset ROM Configuration");
}

void PrNetRomManager::printConfig()
{
    Serial.println(SETTINGS_FLASH_PAGE);
    prnetConfig *p = (prnetConfig *)ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
    Serial.print("pageCounter: ");
    Serial.println(p->pageCounter);
    Serial.print("rowCounter: ");
    Serial.println(p->rowCounter);
    Serial.print("deviceID: ");
    Serial.println(p->deviceID);
}