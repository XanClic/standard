#ifndef EVENTS_H
#define EVENTS_H

#include <stdbool.h>


typedef enum event_type
{
    EVENT_NORMAL_KEY,
    EVENT_INSERT_KEY
} event_type_t;

typedef struct event
{
    event_type_t type;
    int code;
} event_t;

typedef struct event_handler
{
    event_t event;

    bool (*handler)(const event_t *event, void *info);
    void *info;
} event_handler_t;


bool trigger_event(event_t event);
void register_event_handler(event_t event, bool (*handler)(const event_t *event, void *info), void *info);

#endif
