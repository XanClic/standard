#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"


#define EVENT_HASH_BITS 8


static struct event_handler_list
{
    struct event_handler_list *next;
    event_handler_t handler;
} *event_handlers[1 << EVENT_HASH_BITS];


static int hash_event(const event_t *event)
{
    if (event->type != EVENT_NORMAL_KEY_SEQ)
        return (event->type + event->code) & ((1 << EVENT_HASH_BITS) - 1);
    else
    {
        int sum = event->type;
        for (int i = 0; event->key_seq[i]; i++)
            sum += event->key_seq[i];
        return sum & ((1 << EVENT_HASH_BITS) - 1);
    }
}


static bool key_seq_cmp(const int *ks1, const int *ks2)
{
    for (int i = 0; ks1[i] || ks2[i]; i++)
        if (ks1[i] != ks2[i])
            return false;

    return true;
}


bool trigger_event(event_t event)
{
    int hash = hash_event(&event);

    if (event.type == EVENT_NORMAL_KEY_SEQ)
    {
        // The if statement has to check the type first, since key_seq_cmp will dereference an otherwise invalid pointer
        for (struct event_handler_list *ehl = event_handlers[hash]; ehl != NULL; ehl = ehl->next)
            if ((ehl->handler.event.type == EVENT_NORMAL_KEY_SEQ) && key_seq_cmp(ehl->handler.event.key_seq, event.key_seq))
                return ehl->handler.handler(&event, ehl->handler.info);
    }
    else
    {
        for (struct event_handler_list *ehl = event_handlers[hash]; ehl != NULL; ehl = ehl->next)
            if ((ehl->handler.event.code == event.code) && (ehl->handler.event.type == event.type))
                return ehl->handler.handler(&event, ehl->handler.info);
    }

    return false;
}


void register_event_handler(event_t event, bool (*handler)(const event_t *event, void *info), void *info)
{
    int hash = hash_event(&event);

    for (struct event_handler_list *ehl = event_handlers[hash]; ehl != NULL; ehl = ehl->next)
    {
        if ((ehl->handler.event.type == event.type) &&
           (((event.type == EVENT_NORMAL_KEY_SEQ) && key_seq_cmp(ehl->handler.event.key_seq, event.key_seq)) ||
            ((event.type != EVENT_NORMAL_KEY_SEQ) && (ehl->handler.event.code == event.code))))
        {
            if (event.type == EVENT_NORMAL_KEY_SEQ)
                free((void *)event.key_seq);

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
