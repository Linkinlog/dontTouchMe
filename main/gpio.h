#ifndef GPIO_H
#define GPIO_H
#include "hal/adc_types.h"

void gpio_init_led(void);
void stop_blink_led(void);

typedef struct {
  int value;
  int samples; // Number of samples to average
  adc_unit_t unit;
  adc_channel_t channel;
} adc_config_t;

int read_adc_filtered(adc_config_t *config);

typedef struct {
  adc_unit_t unit;
  adc_channel_t channel;
} gpio_queue_item_t;

void gpio_queue_init(void);
void add_sensor(adc_channel_t channel, adc_unit_t unit);

#endif
