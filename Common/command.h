#ifndef COMMAND_H
#define COMMAND_H

// Set to non-zero if mother node should request data for sensor on next
// collection interval
extern uint8_t pendingDataRequestForSensorId;

void InterpretCommand();

#endif
