#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>


void editor(void);

void full_redraw(void);

void update_active_buffer(void);
void reposition_cursor(bool update_desire);

void error(const char *format, ...);

#endif
