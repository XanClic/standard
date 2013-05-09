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


static struct termios initial_tios;


static void term_release(void)
{
    term_clear();

    tcsetattr(STDIN_FILENO, TCSANOW, &initial_tios);
}


void term_init(void)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    term_width  = ws.ws_col;
    term_height = ws.ws_row;

    atexit(term_release);

    tcgetattr(STDIN_FILENO, &initial_tios);

    struct termios tios;
    memcpy(&tios, &initial_tios, sizeof(tios));
    tios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
    tios.c_cc[VTIME] = 1;
    tios.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &tios);

    setbuf(stdin, NULL);
    setvbuf(stdout, NULL, _IOFBF, 0);
}


void term_clear(void)
{
    print("\033[2J\033[H");
}


void term_underline(bool ul)
{
    print(ul ? "\033[4m" : "\033[24m");
}


void term_bold(bool bold)
{
    print(bold ? "\033[1m" : "\033[22m");
}


void term_set_color(color_t fg, color_t bg)
{
    switch (fg.type)
    {
        case COL_DEFAULT:
            print("\033[22;39m");
            break;

        case COL_8:
            printf("\033[3%im", fg.color & 7);
            break;

        case COL_16:
            if (fg.color & 8)
                printf("\033[1;3%im", fg.color & 7);
            else
                printf("\033[22;3%im", fg.color & 7);
            break;

        case COL_256:
            printf("\033[38;5;%im", fg.color);
            break;

        case COL_RGB:
            printf("\033[38;2;%i;%i;%im", fg.r, fg.g, fg.b);
    }

    switch (bg.type)
    {
        case COL_DEFAULT:
            print("\033[49m");
            break;

        case COL_8:
        case COL_16:
            printf("\033[4%im", bg.color & 7);
            break;

        case COL_256:
            printf("\033[48;5;%im", bg.color);
            break;

        case COL_RGB:
            printf("\033[48;2;%i;%i;%im", bg.r, bg.g, bg.b);
    }
}


void term_cursor_pos(int x, int y)
{
    y += x / term_width;
    x %= term_width;

    printf("\033[%i;%iH", y + 1, x + 1);
    fflush(stdout);
}
