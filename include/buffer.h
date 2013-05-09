#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>


typedef struct buffer
{
    // Tab name
    char *name;
    // File location
    char *location;
    // Total line count
    int line_count;
    // Line array
    char **lines;
    // Y locations of the every lines on screen (only valid if visible)
    int *line_screen_pos;
    // X and Y location of cursor (X in utf8 characters)
    int x, y;
    // First and last line on screen (only valid if active)
    int ys, ye;
    // Width of line numbering field
    int linenr_width;
    // True iff content has been modified
    bool modified;
    // Number of screen lines at bottom belonging to a line too long to be displayed (only valid if visible)
    // (OLL = overly long line)
    int oll_unused_lines;
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

void buffer_insert(buffer_t *buf, const char *string);

void buffer_list_append(buffer_t *buf);

void buffer_activate_next(void);
void buffer_activate_prev(void);

#endif
