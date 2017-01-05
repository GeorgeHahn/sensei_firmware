// Config
#define NETWORK_SIZE			64
#define TX_POWER_LEVEL			-12

#define MS_SEND_DELAY_MIN		5
#define MS_SEND_DELAY_MAX		50
#define SECONDS_TO_COLLECT		1
#define SECONDS_TO_TRACK		15
#define SECONDS_TO_ACK_TIME		60
#define SECONDS_TO_TRANSFER		5
#define MINUTES_BETWEEN_SYNC	20

// Radio commands
#define COMMAND_SHARED_TIME 254
#define COMMAND_RESPONSE_ROWS 255

// RSSI total and count for each device
extern int rssiTotal[];
extern int rssiCount[];

// Boolean on whether radio stack is on
extern bool broadcasting;

// Boolean on whether received new data
extern bool newData;
// Boolean on whether to collect data
extern bool collectData;
// Time acknowledgment variable
extern unsigned long discoveryTime;

void startBroadcast();
void stopBroadcast();
uint8_t getDeviceIndex(uint32_t esn);
uint16_t getDeviceUid(uint32_t esn);