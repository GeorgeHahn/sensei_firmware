#include <SimbleeCOM.h>
#include "src/Common/rom.h"
#include "src/Common/rtc.h"
#include "src/Common/accel.h"
#include "src/Common/radio_common.h"
#include "src/Common/debug.h"

extern volatile bool ready_for_next_page;

uint16_t GetTime();
void RequestROMFull(uint8_t id);
void remoteEraseROM();
void resetROM();
void writeData();
void writeDataRow(uint8_t data);
void sendROMPage(uint8_t pageNumber);
void sendPageInfo();
void sendROM();
void sendROM_heatshrink();
