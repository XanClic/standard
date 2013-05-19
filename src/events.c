#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"


#define EVENT_HASH_BITS 4


static struct event_handler_list
{
    struct event_handler_list *next;
    event_handler_t handler;
} *event_handlers[1 << EVENT_HASH_BITS];


static int hash_event(const event_t *event)
{
    return (event->type + event->code) & ((1 << EVENT_HASH_BITS) - 1);
}


bool trigger_event(event_t event)
{
    int hash = hash_event(&event);

    for (struct event_handler_list *ehl = event_handlers[hash]; ehl != NULL; ehl = ehl->next)
        if ((ehl->handler.event.code == event.code) && (ehl->handler.event.type == event.type))
            return ehl->handler.handler(&event, ehl->handler.info);

    return false;
}


void register_event_handler(event_t event, bool (*handler)(const event_t *event, void *info), void *info)
{
    int hash = hash_event(&event);

    for (struct event_handler_list *ehl = event_handlers[hash]; ehl != NULL; ehl = ehl->next)
    {
        if ((ehl->handler.event.code == event.code) && (ehl->handler.event.type == event.type))
        {
            ehl->handler.handler = handler;
            ehl->handler.info = info;
            return;
        }
    }

    struct event_handler_list *ehl = malloc(sizeof(*ehl));
    memcpy(&ehl->handler.event, &event, sizeof(event));
    ehl->handler.handler = handler;
    ehl->handler.info = info;

    ehl->next = event_handlers[hash];
    event_handlers[hash] = ehl;
}
