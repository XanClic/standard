#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "term.h"


int term_width, term_height;


void print(const char *s)
{
    fwrite(s, 1, strlen(s), stdout);
}


void print_flushed(const char *s)
{
    print(s);
    fflush(stdout);
}


void term_init(void)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    term_width  = ws.ws_col;
    term_height = ws.ws_row;

    atexit(term_clear);
}


void term_clear(void)
{
    print_flushed("\033[2J\033[H");
}


void term_underline(bool ul)
{
    print_flushed(ul ? "\033[4m" : "\033[24m");
}


void term_cursor_pos(int x, int y)
{
    y += x / term_width;
    x %= term_width;

    printf("\033[%i;%iH", y + 1, x + 1);
    fflush(stdout);
}
