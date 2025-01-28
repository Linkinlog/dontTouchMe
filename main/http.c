#include "http.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/idf_additions.h"
#include <sys/param.h>
#include <time.h>

#define HTTP_ENDPOINT CONFIG_HTTP_ENDPOINT
#define HTTP_PATH CONFIG_HTTP_PATH
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define HTTP_QUEUE_SIZE 10
#define MAX_CONNECTIONS 3
#define CONNECTION_TIMEOUT_MS 60000

static const char *TAG = "HTTP_CLIENT";
static http_connection_t connection_pool[MAX_CONNECTIONS] = {0};
static SemaphoreHandle_t pool_mutex;

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  static char *output_buffer;

  static int output_len;
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    if (output_len == 0 && evt->user_data) {
      memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
    }

    if (!esp_http_client_is_chunked_response(evt->client)) {
      int copy_len = 0;
      if (evt->user_data) {
        copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
        if (copy_len) {
          memcpy(evt->user_data + output_len, evt->data, copy_len);
        }
      } else {
        int content_len = esp_http_client_get_content_length(evt->client);
        if (output_buffer == NULL) {
          output_buffer = (char *)calloc(content_len + 1, sizeof(char));
          output_len = 0;
          if (output_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
            return ESP_FAIL;
          }
        }
        copy_len = MIN(evt->data_len, (content_len - output_len));
        if (copy_len) {
          memcpy(output_buffer + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;
    }

    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    if (output_buffer != NULL) {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
    int mbedtls_err = 0;
    esp_err_t err = esp_tls_get_and_clear_last_error(
        (esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
    if (err != 0) {
      ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
      ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
    }
    if (output_buffer != NULL) {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    esp_http_client_set_header(evt->client, "Accept", "text/html");
    esp_http_client_set_redirection(evt->client);
    break;
  }
  return ESP_OK;
}

static esp_http_client_handle_t create_connection(void) {
  esp_http_client_config_t config = {
      .url = "https://" HTTP_ENDPOINT HTTP_PATH,
      .event_handler = http_event_handler,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .disable_auto_redirect = true,
      .timeout_ms = 5000,
      .keep_alive_enable = true,
  };

  ESP_LOGI(TAG, "client created");
  return esp_http_client_init(&config);
}

void init_connection_pool(void) {
  pool_mutex = xSemaphoreCreateMutex();

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    connection_pool[i].client = create_connection();
    connection_pool[i].in_use = false;
    connection_pool[i].last_used = 0;
  }
}

static http_connection_t *get_connection(void) {
  xSemaphoreTake(pool_mutex, portMAX_DELAY);

  int64_t current_time = time(NULL) * 1000;
  http_connection_t *selected_conn = NULL;

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (!connection_pool[i].in_use) {
      if (current_time - connection_pool[i].last_used > CONNECTION_TIMEOUT_MS) {
        esp_http_client_cleanup(connection_pool[i].client);
        connection_pool[i].client = create_connection();
      }
      connection_pool[i].in_use = true;
      connection_pool[i].last_used = current_time;
      selected_conn = &connection_pool[i];
      break;
    }
  }

  xSemaphoreGive(pool_mutex);
  return selected_conn;
}

static void release_connection(http_connection_t *conn) {
  xSemaphoreTake(pool_mutex, portMAX_DELAY);
  conn->in_use = false;
  conn->last_used = time(NULL) * 1000;
  xSemaphoreGive(pool_mutex);
}

void post_data(char post_data[64]) {
  http_connection_t *conn = get_connection();
  if (conn == NULL) {
    ESP_LOGE(TAG, "Failed to get connection from pool");
    return;
  }

  esp_http_client_handle_t client = conn->client;

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, post_data, strlen(post_data));

  esp_err_t err;
  while (1) {
    err = esp_http_client_perform(client);
    if (err != ESP_ERR_HTTP_EAGAIN) {
      break;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  } else {
    ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
  }

  release_connection(conn);
}

static QueueHandle_t http_queue = NULL;

void http_task_handler(void *pvParameters) {
  http_queue_item_t item;
  while (1) {
    if (xQueueReceive(http_queue, &item, portMAX_DELAY) == pdTRUE) {
      char data[64];
      snprintf(data, sizeof(data), "{\"content\":\"I've been touched!!\"}");

      post_data(data);
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void http_queue_init(void) {
  http_queue = xQueueCreate(HTTP_QUEUE_SIZE, sizeof(http_queue_item_t));

  char data[64];
  snprintf(data, sizeof(data), "{\"content\":\"Sensor Initialized\"}");

  post_data(data);
  xTaskCreate(http_task_handler, "http_task_handler", 4096, NULL, 5, NULL);
}

void http_task_send(void) {
  http_queue_item_t *item = {};

  if (xQueueSend(http_queue, item, 0) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to send item to http queue");
  }
}
