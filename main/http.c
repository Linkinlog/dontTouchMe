#include "http.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/idf_additions.h"
#include <sys/param.h>

#define HTTP_ENDPOINT CONFIG_HTTP_ENDPOINT
#define HTTP_PATH CONFIG_HTTP_PATH
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define HTTP_QUEUE_SIZE 10
#define MAX_CONNECTIONS 3
#define CONNECTION_TIMEOUT_MS 5000

static const char *TAG = "HTTP_CLIENT";
static connection_pool_t pool = {0};

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

void connection_pool_init(void) {
  pool.lock = xSemaphoreCreateMutex();

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    esp_http_client_config_t config = {
        .url = "https://" HTTP_ENDPOINT HTTP_PATH,
        .event_handler = http_event_handler,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
        .timeout_ms = 5000,
    };

    pool.connections[i].client = esp_http_client_init(&config);
    pool.connections[i].in_use = false;
    pool.connections[i].last_used = 0;
  }
}

esp_http_client_handle_t get_connection(void) {
  if (xSemaphoreTake(pool.lock, portMAX_DELAY) != pdTRUE) {
    return NULL;
  }

  time_t now = time(NULL);
  http_connection_t *best_connection = NULL;

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (!pool.connections[i].in_use) {
      best_connection = &pool.connections[i];
      break;
    }
  }

  if (!best_connection) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      if (!pool.connections[i].last_used * 1000 > CONNECTION_TIMEOUT_MS) {
        best_connection = &pool.connections[i];
        break;
      }
    }
  }

  if (best_connection) {
    best_connection->in_use = true;
    best_connection->last_used = now;
  }

  xSemaphoreGive(pool.lock);
  return best_connection ? best_connection->client : NULL;
}

void release_connection(esp_http_client_handle_t client) {
  if (xSemaphoreTake(pool.lock, portMAX_DELAY) != pdTRUE) {
    return;
  }

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (pool.connections[i].client == client) {
      pool.connections[i].in_use = false;
      pool.connections[i].last_used = time(NULL);
      break;
    }
  }

  xSemaphoreGive(pool.lock);
}

void post_data(http_queue_item_t *item) {
  esp_http_client_handle_t client = get_connection();
  char post_data[64];
  snprintf(post_data, sizeof(post_data),
           "{\"content\":\"I've been touched!!, timestamp :%lld\"}",
           item->timestamp);

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, post_data, strlen(post_data));

  esp_err_t err;
  while (1) {
    err = esp_http_client_perform(client);
    if (err != ESP_ERR_HTTP_EAGAIN) {
      break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  } else {
    ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
}

static QueueHandle_t http_queue = NULL;

void http_task_handler(void *pvParameters) {
  http_queue_item_t item;
  while (1) {
    if (xQueueReceive(http_queue, &item, portMAX_DELAY) == pdTRUE) {
      post_data(&item);
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void http_queue_init(void) {
  http_queue = xQueueCreate(HTTP_QUEUE_SIZE, sizeof(http_queue_item_t));

  xTaskCreate(http_task_handler, "http_task_handler", 4096, NULL, 5, NULL);
}

void http_task_send(http_queue_item_t *item) {
  if (xQueueSend(http_queue, item, 0) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to send item to http queue");
  }
}
