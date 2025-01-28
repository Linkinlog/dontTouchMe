#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "gpio.h"
#include "hal/adc_types.h"
#include "http.h"
#include "nvs_flash.h"
#include "wifi.h"
#include <sys/param.h>

static adc_channel_t channel = ADC_CHANNEL_0;
static adc_unit_t unit = ADC_UNIT_1;
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

void app_main(void) {
  ESP_LOGI(TAG, "ESP_LED");
  gpio_init_led();
  ESP_LOGI(TAG, "ESP_NVS");
  nvs_init();
  ESP_LOGI(TAG, "ESP_WIFI");
  wifi_init_sta();
  ESP_LOGI(TAG, "ESP_POOL");
  init_connection_pool();
  ESP_LOGI(TAG, "ESP_HTTP");
  http_queue_init();
  ESP_LOGI(TAG, "ESP_GPIO");
  gpio_queue_init();

  add_sensor(channel, unit);

  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_LOGI(TAG, "ESP_SETUP_FINISH");
  stop_blink_led();

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
