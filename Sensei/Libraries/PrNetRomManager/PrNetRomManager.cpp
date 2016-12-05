/*
  PrNetRomManager.h - Library for managing data in the flash rom of PrNet nodes.
  Created by Dwyane George, March 12, 2016, Adapted version from Nazmus Saquib, October 7, 2015
  Social Computing Group, MIT Media Lab
*/

#include "Arduino.h"
#include "PrNetRomManager.h"

PrNetRomManager::PrNetRomManager() {
  loadConfig();
}

void PrintHexInt(uint32_t data)
{
	char tmp[9];
	for (int8_t i=7; i >= 0; i--)
	{
		uint8_t temp = data & 0x0F;
		if(temp < 0x0A)
			tmp[i] = '0' + temp;
		else
			tmp[i] = 'A' - 0xA + temp;
		data >>= 4;
	}
	tmp[8] = 0;
	Serial.print(tmp);
}

void PrintHexByte(uint8_t data)
{
	char tmp[3];
	for (int8_t i=1; i >= 0; i--)
	{
		uint8_t temp = data & 0x0F;
		if(temp < 0x0A)
			tmp[i] = '0' + temp;
		else
			tmp[i] = 'A' - 0xA + temp;
		data >>= 4;
	}
	tmp[2] = 0;
	Serial.print(tmp);
}

void PrNetRomManager::printPage(int page) {
	data *p = (data*) ADDRESS_OF_PAGE(page);
	for (int i = 0; i < MAX_ROWS; i++) {
		if ((p -> data[i]) == 0xFFFFFFFF) {
			
		} else {
			PrintHexByte(page);
			PrintHexByte(i);
			Serial.print(": ");
			PrintHexInt(p -> data[i]);
			Serial.println();
		}
	}
}

void PrNetRomManager::printROM() {
  for (int i = STORAGE_FLASH_PAGE; i >= LAST_STORAGE_PAGE; i--) {
    printPage(i);
    data *p = (data*) ADDRESS_OF_PAGE(i);
    if ((p -> data[0]) == -1) {
      //break;
    }
  }
  Serial.println("Printed ROM");
}

void PrNetRomManager::loadPage(int page) {
  data *p = (data*) ADDRESS_OF_PAGE(page);
  for (int i = 0; i < MAX_ROWS; i++) {
    table.data[i] = p -> data[i];
  }
  loadedPage = page;
}

int PrNetRomManager::erasePage(int page) {
  return flashPageErase(page);
}

void PrNetRomManager::eraseROM() {
  for (int i = SETTINGS_FLASH_PAGE; i >= LAST_STORAGE_PAGE; i--) {
    erasePage(i);
  }
  Serial.println("Erased ROM");
}

/*
 *  Clears the transfer data array
 */
void PrNetRomManager::clearTransferredData() {
  for (int i = 0; i < MAX_ROWS; i++) {
    transferredData.data[i] = 0;
  }
}

int PrNetRomManager::writePage(int page, struct data values) {
  if (page >= LAST_STORAGE_PAGE) {
    data *p = (data*) ADDRESS_OF_PAGE(page);
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

void PrNetRomManager::loadConfig() {
  prnetConfig *p = (prnetConfig*) ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
  config.pageCounter = p -> pageCounter;
  config.rowCounter = p -> rowCounter;
}

int PrNetRomManager::updateConfig() {
  erasePage(SETTINGS_FLASH_PAGE);
  prnetConfig *p = (prnetConfig*) ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
  return flashWriteBlock(p, &config, sizeof(config));
}

void PrNetRomManager::resetConfig() {
  config.pageCounter = STORAGE_FLASH_PAGE;
  config.rowCounter = 0;
  updateConfig();
  Serial.println("Reset ROM Configuration");
}

void PrNetRomManager::printConfig() {
  Serial.println(SETTINGS_FLASH_PAGE);
  prnetConfig *p = (prnetConfig*) ADDRESS_OF_PAGE(SETTINGS_FLASH_PAGE);
  Serial.println(p -> pageCounter);
  Serial.println(p -> rowCounter);
}