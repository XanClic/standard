#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "editor.h"
#include "term.h"
#include "tools.h"
#include "utf8.h"


void editor(void)
{
    full_redraw();

    sleep(3);
}


void full_redraw(void)
{
    term_clear();


    int position = 1;
    term_underline(true);
    putchar(' ');

    for (buffer_list_t *bl = buffer_list; bl != NULL; bl = bl->next)
    {
        buffer_t *buf = bl->buffer;

        term_underline(buf != active_buffer);
        printf("/ %s \\", buf->name);

        position += 2 + utf8_strlen(buf->name) + 2;
    }


    int remaining = (16 * term_width - position) % term_width; // FIXME

    term_underline(true);
    printf("%*s", remaining, "");
    term_underline(false);


    int y_pos = 1, line;

    for (line = active_buffer->ys; (y_pos < term_height - 2) && (line < active_buffer->line_count); line++)
    {
        printf(" %*i %s\n", active_buffer->linenr_width, line, active_buffer->lines[line]);
        y_pos += (utf8_strlen(active_buffer->lines[line]) + 1 + active_buffer->linenr_width + 1 + term_width - 1) / term_width;
    }

    while (y_pos++ < term_height - 2)
        printf(" %*s ~\n", active_buffer->linenr_width, "-");


    printf("%-*s", term_width - 16, active_buffer->location);
    position = printf("%i,%i", active_buffer->x, active_buffer->y);

    printf("%*c", 13 - position, ' ');

    bool top = !active_buffer->ys;
    bool bot = (line >= active_buffer->line_count - 1);

    if (top && bot)
        print("All");
    else if (top)
        print("Top");
    else if (bot)
        print("Bot");
    else
        printf("%2i%%", (active_buffer->ys * 100) / (active_buffer->line_count - line + active_buffer->ys));


    term_cursor_pos(1 + active_buffer->linenr_width + 1 + active_buffer->x, 1 + active_buffer->y - active_buffer->ys);
}
