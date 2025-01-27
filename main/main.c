#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "gpio.h"
#include "hal/adc_types.h"
#include "http.h"
#include "nvs_flash.h"
#include "soc/gpio_num.h"
#include "wifi.h"
#include <string.h>
#include <sys/param.h>
#include <time.h>

static adc_channel_t channel = ADC_CHANNEL_0;
static adc_unit_t unit = ADC_UNIT_1;
static const char *TAG = "main STA";
static gpio_num_t led_gpio = GPIO_NUM_25;

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
  ESP_LOGI(TAG, "ESP_NVS");
  nvs_init();
  ESP_LOGI(TAG, "ESP_WIFI");
  wifi_init_sta();
  ESP_LOGI(TAG, "ESP_GPIO");
  gpio_init_led(led_gpio);
  ESP_LOGI(TAG, "ESP_HTTP");
  http_queue_init();

  // blink_led(led_gpio);

  int last_raw = 0;

  adc_config_t config = {
      .value = 0,
      .samples = 10,
      .unit = unit,
      .channel = channel,
  };

  while (1) {
    int raw = read_adc_filtered(&config);
    if (raw != last_raw) {
      ESP_LOGI(TAG, "ADC[%d]: %d", channel, raw);

      http_queue_item_t item = {
          .timestamp = time(NULL),
      };

      http_task_send(&item);
      last_raw = raw;
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
