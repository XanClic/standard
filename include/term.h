#ifndef TERM_H
#define TERM_H

#include <stdbool.h>
#include <stdint.h>


enum color_type
{
    COL_DEFAULT = 0,
    COL_8,
    COL_16,
    COL_256,
    COL_RGB
};

typedef struct color
{
    enum color_type type;
    union
    {
        uint8_t color;
        struct
        {
            uint8_t r, g, b;
        };
    };
} color_t;


extern int term_width, term_height;

#define buffer_width  (term_width)
#define buffer_height (term_height - 3)


void term_init(void);
void term_release(void);

void print(const char *s);
void print_flushed(const char *s);

void term_clear(void);
void term_underline(bool ul);
void term_bold(bool bold);
void term_set_color(color_t fg, color_t bg);

void term_cursor_pos(int x, int y);

#endif
