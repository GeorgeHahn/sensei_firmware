#define ACCEL_POWER_PIN			23
#define X_PIN					4
#define Z_PIN					6

// TODO:
// Y_PIN unrouted due to PCB layout, consider adding iter_swap
// Consider adding analog low pass filtering on the accel pin

// Accelerometer variables
extern bool accelerometerOn;
extern int xAccelerometerPrev;
extern int zAccelerometerPrev;
extern int xAccelerometerDiff;
extern int zAccelerometerDiff;
extern int accelerometerCount;

////////// Accelerometer Functions //////////

void setupAccelerometer();
void enableAccelerometer();
void disableAccelerometer();
void readAcc();
void clearAccelerometerState();
