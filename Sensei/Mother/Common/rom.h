#include "PrNetRomManager\PrNetRomManager.h"
#include "PrNetRomManager\util.h"

#define ROW_RESET	(0)
#define ROW_TIME	(1)
#define ROW_ACCEL	(2)
#define ROW_PROX	(3)

// Flash storage headers - 1-3 bits
#define DATA_ROW_HEADER 0x80000000 // 0b1... (31 bits)
#define ACCEL_ROW_HEADER 0x00000000 // 0b000... (29 bits)

// NOT CURRENTLY USED; TODO consider periodically storing the full time and then only recording offsets elsewhere
#define TIME_ROW_HEADER 0x40000000 // 0b010... (29 bits)

#define SPECIAL_ROW_HEADER 0x20000000 // 0b001... (29 bits)
#define RESET_ROW_HEADER 0x3B9AC9FF // header == SPECIAL_ROW_HEADER; RESET_ROW_HEADER = 999999999

// Reserved for future use
#define RESERVED_ROW_HEADER 0x30000000 // 0b011... (29 bits)

// TODO: Maybe record the full date when the time gets set?

extern PrNetRomManager romManager;

// Data transfer protocol state variables
extern int transferPage;
extern int transferRow;
extern bool transferPageSuccess;
extern bool lastPage;

extern unsigned long lastTransferTime;
extern unsigned long dataTransferTime;

uint16_t GetTime();
void writeData();
void writeDataRow(uint8_t data);
void sendROMRequest();
void sendROMResponse();
void remoteEraseROM();
void resetROM();