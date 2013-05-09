#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>


typedef struct buffer
{
    char *name;
    char *location;
    int line_count;
    char **lines;
    int x, y, ys;
    int linenr_width;
} buffer_t;


typedef struct buffer_list
{
    struct buffer_list *next;
    buffer_t *buffer;
} buffer_list_t;


extern buffer_list_t *buffer_list;
extern buffer_t *active_buffer;


bool load_buffer(const char *file);
void buffer_destroy(buffer_t *buf);

void buffer_list_append(buffer_t *buf);

#endif
