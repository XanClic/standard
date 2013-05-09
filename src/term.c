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
