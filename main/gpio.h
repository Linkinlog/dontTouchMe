#ifndef GPIO_H
#define GPIO_H
#include "hal/adc_types.h"
#include "soc/gpio_num.h"

int read_adc(adc_unit_t unit, adc_channel_t channel);

void blink_led(gpio_num_t gpio_num);
void configure_led(gpio_num_t gpio_num);

#endif
