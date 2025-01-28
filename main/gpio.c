#include "gpio.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "http.h"
#include "portmacro.h"

#define GPIO_QUEUE_SIZE 10

static gpio_num_t led_gpio = GPIO_NUM_25;
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

static bool is_blinking = false;

void blink_led_task() {
  is_blinking = true;
  while (is_blinking) {
    gpio_set_level(led_gpio, s_led_state);
    vTaskDelay(pdMS_TO_TICKS(50));
    s_led_state = !s_led_state;
  }
  if (!is_blinking) {
    gpio_set_level(led_gpio, 0);
  }

  vTaskDelete(NULL);
}

void stop_blink_led(void) { is_blinking = false; }

void gpio_init_led() {
  ESP_LOGI(TAG, "Configuring LED on GPIO %d", led_gpio);
  gpio_reset_pin(led_gpio);

  gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);

  xTaskCreate(blink_led_task, "blink_led", 2048, NULL, 1, NULL);
}

void process_gpio(adc_channel_t channel, adc_unit_t unit) {
  adc_config_t config = {
      .value = 0,
      .samples = 10,
      .unit = unit,
      .channel = channel,
  };

  int last_raw = read_adc_filtered(&config);
  static TickType_t last_send_time = 0;

  while (1) {
    int raw = read_adc_filtered(&config);
    if (raw != last_raw) {

      TickType_t current_time = xTaskGetTickCount();
      if (current_time - last_send_time >= pdMS_TO_TICKS(5000)) {
        ESP_LOGI(TAG, "ADC[%d]: %d", channel, raw);
        http_task_send();
        last_send_time = current_time;
      } else {
        ESP_LOGI(TAG, "ADC[%d](SKIP): %d", channel, raw);
      }

      last_raw = raw;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static QueueHandle_t gpio_queue = NULL;

void gpio_task_handler(void *pvParameters) {
  gpio_queue_item_t item;
  while (1) {
    if (xQueueReceive(gpio_queue, &item, portMAX_DELAY) != pdTRUE) {
      ESP_LOGW(TAG, "QUEUE full");
      return;
    }
    process_gpio(item.channel, item.unit);
  }
}

void gpio_queue_init(void) {
  gpio_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_queue_item_t));
  xTaskCreate(gpio_task_handler, "gpio_task_handler", 4096, NULL, 5, NULL);
}

void add_sensor(adc_channel_t channel, adc_unit_t unit) {
  gpio_queue_item_t item = {
      .channel = channel,
      .unit = unit,
  };

  if (xQueueSend(gpio_queue, &item, 0) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to send item to GPIO queue");
  }
}
