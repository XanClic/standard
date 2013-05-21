#ifndef EVENTS_H
#define EVENTS_H

#include <stdbool.h>


typedef enum event_type
{
    EVENT_NORMAL_KEY,
    EVENT_INSERT_KEY,
    EVENT_MBUTTON_DOWN,
    EVENT_MBUTTON_UP
} event_type_t;

typedef struct event
{
    event_type_t type;
    // Primary code for registering handlers
    int code;

    // Supplementary information
    // union
    // {
        struct
        {
            int x, y;
        } mbutton;
    // };
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
