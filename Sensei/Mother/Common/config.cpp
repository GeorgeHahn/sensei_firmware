#include <Arduino.h>
#include <SimbleeRNG.h>

void RandomSeed()
{
    NRF_RNG->TASKS_STOP = 1;
    NRF_RNG->CONFIG = (RNG_CONFIG_DERCEN_Enabled << RNG_CONFIG_DERCEN_Pos);
    NRF_RNG->TASKS_START = 1;

    NRF_RNG->EVENTS_VALRDY = 0;
    while (!NRF_RNG->EVENTS_VALRDY)
        ;
    uint8_t firstval = NRF_RNG->VALUE;

    NRF_RNG->EVENTS_VALRDY = 0;
    while (!NRF_RNG->EVENTS_VALRDY)
        ;
    randomSeed(firstval << 8 || NRF_RNG->VALUE);

    NRF_RNG->TASKS_STOP = 1;
}