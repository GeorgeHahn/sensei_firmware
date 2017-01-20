/*
  PrNetRomManager.h - Library for managing data in the flash rom of PrNet nodes.
  Created by Dwyane George, March 12, 2016, Adapted version from Nazmus Saquib, October 7, 2015
  Social Computing Group, MIT Media Lab
*/

#ifndef PrNetRomManager_h
#define PrNetRomManager_h

#include "Arduino.h"

#define SETTINGS_FLASH_PAGE 251
#define STORAGE_FLASH_PAGE 250
#define LAST_STORAGE_PAGE 124
#define MAX_ROWS 256

struct data {
    unsigned int data[MAX_ROWS];
};

struct prnetConfig {
    int pageCounter = STORAGE_FLASH_PAGE;
    int rowCounter = 0;
    uint8_t deviceID = 0;
};

class PrNetRomManager
{
  public:
    struct data table;
    struct prnetConfig config;
    int loadedPage;
    PrNetRomManager();
    void printPage(int page);
    void printROM();
    void loadPage(int page);
    int erasePage(int page);
    void eraseROM();
    int writePage(int page, struct data values);
    void loadConfig();
    void SetDeviceID(uint8_t newID);
    int updateConfig();
    void resetConfig();
    void printConfig();

    bool OutOfSpace();
    void CheckPageSpace();
};

extern PrNetRomManager romManager;

#endif
