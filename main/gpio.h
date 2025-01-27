#ifndef GPIO_H
#define GPIO_H
#include "hal/adc_types.h"
#include "soc/gpio_num.h"

void blink_led(gpio_num_t gpio_num);
void gpio_init_led(gpio_num_t gpio_num);

typedef struct {
  int value;
  int samples; // Number of samples to average
  adc_unit_t unit;
  adc_channel_t channel;
} adc_config_t;

int read_adc_filtered(adc_config_t *config);

#endif
