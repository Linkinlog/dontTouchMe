#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "nvs_flash.h"
#include <string.h>

// This uses the WiFi configuration that you can set via project configuration
// menu.
#define ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

#define H2E_IDENTIFIER "ESP32"
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK

static int adc_raw[2][10];
static adc_channel_t channel = ADC_CHANNEL_0;

static const char *TAG = "main STA";

void nvs_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

adc_oneshot_unit_handle_t adc_oneshot(void) {
  adc_oneshot_unit_handle_t handle = NULL;
  adc_oneshot_unit_init_cfg_t adc_config = {
      .unit_id = ADC_UNIT_1,
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

void app_main(void) {
  nvs_init();

  ESP_LOGI(TAG, "ESP_WIFI");

  adc_oneshot_unit_handle_t handle = adc_oneshot();

  while (1) {
    ESP_ERROR_CHECK(adc_oneshot_read(handle, channel, &adc_raw[0][0]));
    ESP_LOGI(TAG, "ADC1 Channel[0] Raw Data: %d", adc_raw[0][0]);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_ERROR_CHECK(adc_oneshot_del_unit(handle));
}
