// Config
#define NETWORK_SIZE 64
#define TX_POWER_LEVEL -12

#define MS_SEND_DELAY_MIN 5
#define MS_SEND_DELAY_MAX 50
#define MS_TO_COLLECT 1000
#define SECONDS_TO_TRACK 15
#define SECONDS_TO_ACK_TIME 60
#define SECONDS_TO_TRANSFER 5
#define MINUTES_BETWEEN_SYNC 20

// Radio commands
#define RADIO_PROX_PING 1
#define RADIO_SHARED_TIME 2
#define RADIO_REQUEST_FULL 3
#define RADIO_REQUEST_PARTIAL 4
#define RADIO_RESPONSE_ROWS 5
#define RADIO_RESPONSE_COMPLETE 6
#define RADIO_REQUEST_ERASE 7
#define RADIO_REQUEST_SLEEP 8
#define RADIO_ENTER_OTA_MODE 9
#define RADIO_START_NEW_TRANSFER 10
#define RADIO_REQUEST_NEXT_PAGE 11
#define RADIO_REQUEST_COMPRESSED 12

// RSSI total and count for each device
extern int rssiTotal[];
extern int rssiCount[];

// Boolean on whether radio stack is on
extern bool broadcasting;

// Boolean on whether received new data
extern volatile bool newData;
// Boolean on whether to collect data
extern volatile bool collectData;
// Time acknowledgment variable
extern unsigned long discoveryTime;

void startBroadcast();
void stopBroadcast();

void RequestPartialData(uint8_t transferDevice, uint8_t row, uint8_t length);
void RequestFullData(uint8_t transferDevice);
void SendTime(uint8_t month, uint8_t date, uint8_t year, uint8_t day, uint8_t hours, uint8_t minutes, uint8_t seconds);
void RequestNextPage(uint8_t id);