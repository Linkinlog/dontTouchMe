#ifndef HTTP_H
#define HTTP_H

#include "esp_http_client.h"
#include <time.h>

typedef struct {
} http_queue_item_t;

typedef struct {
  esp_http_client_handle_t client;
  time_t last_used;
  bool in_use;
} http_connection_t;

void http_queue_init(void);
void http_task_send(void);
void init_connection_pool(void);

#endif
