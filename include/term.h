#ifndef TERM_H
#define TERM_H

#include <stdbool.h>


extern int term_width, term_height;

#define buffer_width  (term_width)
#define buffer_height (term_height - 3)


void term_init(void);

void print(const char *s);
void print_flushed(const char *s);

void term_clear(void);
void term_underline(bool ul);

void term_cursor_pos(int x, int y);

#endif
