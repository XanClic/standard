#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>


enum input_mode
{
    MODE_NORMAL,
    MODE_INSERT,
    MODE_REPLACE
};

extern enum input_mode input_mode;


void editor(void);

void full_redraw(void);

void update_active_buffer(void);
void reposition_cursor(bool update_desire);

void write_string(const char *s);
void delete_chars(int count);

void error(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
