#ifndef HTTP_H
#define HTTP_H

#include <time.h>

typedef struct {
    time_t timestamp;
} http_queue_item_t;

void http_task_init(void);
void http_task_send(http_queue_item_t *item);

#endif
