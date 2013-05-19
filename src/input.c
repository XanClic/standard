#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "keycodes.h"


static size_t fifo_size, fifo_content;
static int *fifo;


// escape sequence translation table 1; TT for '\e[A' etc.
static int eseq_tt1[26] = {
    ['A' - 'A'] = KEY_UP,
    ['B' - 'A'] = KEY_DOWN,
    ['C' - 'A'] = KEY_RIGHT,
    ['D' - 'A'] = KEY_LEFT,
    ['F' - 'A'] = KEY_END,
    ['H' - 'A'] = KEY_HOME
};


static int read_escape_sequence(void)
{
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    char escape_sequence[8];
    int eseq_len = read(STDIN_FILENO, escape_sequence, 7);
    fcntl(STDIN_FILENO, F_SETFL, 0);

    if (eseq_len <= 0)
        return '\e';

    escape_sequence[eseq_len] = 0;

    if (escape_sequence[0] == '[')
    {
        if ((escape_sequence[1] >= 'A') && (escape_sequence[1] <= 'Z'))
            return eseq_tt1[escape_sequence[1] - 'A'];

        if ((escape_sequence[1] == '3') && (escape_sequence[2] == '~'))
            return KEY_DELETE;
    }

    return 0;
}


int input_read(void)
{
    if (fifo_content)
    {
        int v = fifo[0];

        if (--fifo_content)
            memmove(&fifo[0], &fifo[1], fifo_content * sizeof(fifo[0]));

        return v;
    }


    int inp = getchar();

    if (inp == '\e')
        inp = read_escape_sequence();


    return inp;
}


void sim_input(int val)
{
    if (fifo_content + 1 > fifo_size)
        fifo = realloc(fifo, fifo_size + 16 * sizeof(fifo[0]));

    fifo[fifo_content++] = val;
}
