#ifndef GPIO_H
#define GPIO_H
#include "hal/adc_types.h"

int read_adc(adc_unit_t unit, adc_channel_t channel);

#endif
