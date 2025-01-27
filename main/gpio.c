#include "gpio.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"

static const char *TAG = "GPIO";
static uint8_t s_led_state = 0;

adc_oneshot_unit_handle_t adc_oneshot(adc_unit_t unit, adc_channel_t channel) {
  adc_oneshot_unit_handle_t handle = NULL;
  adc_oneshot_unit_init_cfg_t adc_config = {
      .unit_id = unit,
  };

  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &handle));

  adc_oneshot_chan_cfg_t adc_chan_config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };

  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(handle, channel, &adc_chan_config));

  return handle;
}

int oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t channel,
                 int *raw) {

  ESP_ERROR_CHECK(adc_oneshot_read(handle, channel, raw));

  ESP_ERROR_CHECK(adc_oneshot_del_unit(handle));

  return *raw;
}

int read_adc(adc_unit_t unit, adc_channel_t channel) {
  adc_oneshot_unit_handle_t handle = adc_oneshot(unit, channel);
  int raw;
  return oneshot_read(handle, channel, &raw);
}

int read_adc_filtered(adc_config_t *config) {
  int sum = 0;
  for (int i = 0; i < config->samples; i++) {
    sum += read_adc(config->unit, config->channel);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return sum / config->samples;
}

void blink_led(gpio_num_t gpio_num) {

  while (1) {
    gpio_set_level(gpio_num, s_led_state);
    vTaskDelay(pdMS_TO_TICKS(50));
    s_led_state = !s_led_state;
  }
}

void gpio_init_led(gpio_num_t gpio_num) {
  ESP_LOGI(TAG, "Configuring LED on GPIO %d", gpio_num);
  gpio_reset_pin(gpio_num);

  gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
}
