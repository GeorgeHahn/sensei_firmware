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

Example: T1202165180402000 sets the time to Friday Dec 2 2016 1:04:02.000 PM

## Get ESN (unique ID)
Command: 'I'

Returns device's 32 bit unique ID (in hex)


## Print online devices:
Command: 'O'

Returns a comma separated list of device statuses