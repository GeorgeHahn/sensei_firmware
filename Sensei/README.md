# Protocol

## Help
Command: '?'

Prints list of commands and short descriptions


## Program system time:
Command: 'T'

Format: MMDDYYDHHMMSSXXX

 - MM Month
 - DD Date
 - YY Year
 - D Day of week
 - HH Hour
 - MM Minute
 - SS Second
 - XXX Millisecond

Example: T1202165130402000 sets the time to Friday Dec 2 2016 1:04:02.000 PM

## Get device ID
Command: 'I'

Returns the ID stored in local device flash


## Print online devices:
Command: 'O'

Returns a comma separated list of device statuses. Starts at device 0 and continues up to device N.

Example response:

O:0,0,0,0,1,1,0,0,0,0,... (comma separated ASCII '0'/'1')

## Download device data
Command: 'D'

Returns all records from selected device

Format: NN

Where N is an ASCII hex value corresponding to the device that should be downloaded

Response:

Binary data

{pageID, row, data} xN
pageID: [one byte] the flash page being transfered
row: [one byte] the flash row being transfered
data: 13 bytes of data

[Repeated xN for all non-empty flash pages]

### TODO: send a list of all non-empty flash pages (enables single-page retries)

Command: 'S'

Returns a single flash page from selected device

Format: NN, SS

Where N is the device ID and SS is the requested flash page

Response: see 'DF' command response

## Put device to sleep
Command: 'L'

Instructs given device to go to sleep until the next collection period

Format: NN

Where N is an ASCII hex value corresponding to the device that should go to sleep

Response: None

## Set device ID
__Should only be used at the time of intial programming__

Command: 'Z'

Stores the given ID to flash

Format: NN

# Flash Format

Flash is separated into 4 byte rows (every four bytes is a new row)

## Row headers

The first two bits of a row is its header.

0b1: Data row
0b000: Accelerometer row
0b010: Time row (not currently used)
0b011: Reserved for future use
0b001: Special row

## Data row
0b1, Time (13 bits), rssi (7 bits), unused (3 bits), ID (8 bits)
0b1TTTTTTTTTTTTTRRRRRRRUUUIIIIIIII

RSSI is negated. Eg an RSSI of -100 (0b10011100) would be stored as 100 (0b01100100)

ID is an unsigned byte corresponding to the ID of the device seen. Note that the network size is currently limited to 64 devices. 

## Accelerometer row
Accelerometer x, z reading. Each value is 14 bits (mask 0x3FFF):
0b0000ZZZZZZZZZZZZZZXXXXXXXXXXXXXX

## Time row
TBD

## Special row
### Reset row
Value = 0x3B9AC9FF

Indicates the sensor device has been reset
