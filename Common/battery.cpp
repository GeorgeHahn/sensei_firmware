#include <SimbleeCOM.h>
#include "battery.h"

#define MAX_BATTERY_ADC 200
#define MIN_BATTERY_ADC 140

uint8_t get_battery_pct() {
    float val;

    // Configure ADC
    NRF_ADC->CONFIG     = (ADC_CONFIG_RES_8bit                        << ADC_CONFIG_RES_Pos)     |
                          (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos)  |
                          (ADC_CONFIG_REFSEL_VBG                      << ADC_CONFIG_REFSEL_Pos)  |
                          (ADC_CONFIG_PSEL_Disabled                   << ADC_CONFIG_PSEL_Pos)    |
                          (ADC_CONFIG_EXTREFSEL_None                  << ADC_CONFIG_EXTREFSEL_Pos);
    NRF_ADC->EVENTS_END = 0;
    NRF_ADC->ENABLE     = ADC_ENABLE_ENABLE_Enabled;

    NRF_ADC->EVENTS_END  = 0;    // Stop any running conversions.
    NRF_ADC->TASKS_START = 1;

    while (!NRF_ADC->EVENTS_END)

    val = NRF_ADC->RESULT;

    return (uint8_t) ((val - MIN_BATTERY_ADC) / (MAX_BATTERY_ADC - MIN_BATTERY_ADC)) * 100;
}
