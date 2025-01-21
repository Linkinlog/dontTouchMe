#include "gpio.h"
#include "esp_adc/adc_oneshot.h"

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
