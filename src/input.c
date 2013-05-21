#include <fcntl.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "events.h"
#include "input.h"
#include "keycodes.h"


#define ESEQ_HASH_BITS 8


static struct eseq_trans_list
{
    struct eseq_trans_list *next;
    const char *sequence;
    int keycode;
} *eseq_trans_list[1 << ESEQ_HASH_BITS];


static size_t fifo_size, fifo_content;
static int *fifo;


static int hash_eseq(const char *sequence)
{
    uint32_t hash = 5381;
    uint8_t c;

    while ((c = *(sequence++)))
        hash = ((hash << 5) + hash) ^ c;

    return hash & ((1 << ESEQ_HASH_BITS) - 1);
}


static int read_escape_sequence(void)
{
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    char escape_sequence[32] = { '\033' };
    int eseq_len = fread(&escape_sequence[1], 1, 30, stdin);
    fcntl(STDIN_FILENO, F_SETFL, 0);

    if (eseq_len <= 0)
        return '\033';

    int hash = hash_eseq(escape_sequence);

    for (struct eseq_trans_list *etl = eseq_trans_list[hash]; etl != NULL; etl = etl->next)
        if (!strcmp(etl->sequence, escape_sequence))
            return etl->keycode;


    regex_t mouse_input_regex;
    regcomp(&mouse_input_regex, "^\033\\[<\\([0-9]\\+\\);\\([0-9]\\+\\);\\([0-9]\\+\\)\\([mM]\\)$", 0);

    regmatch_t matches[5];
    int result = regexec(&mouse_input_regex, escape_sequence, 5, matches, 0);

    regfree(&mouse_input_regex);
    if (result)
        return 0;


    int button = atoi(&escape_sequence[matches[1].rm_so]);
    int x      = atoi(&escape_sequence[matches[2].rm_so]);
    int y      = atoi(&escape_sequence[matches[3].rm_so]);

    trigger_event((event_t){ (escape_sequence[matches[4].rm_so] == 'M') ? EVENT_MBUTTON_DOWN : EVENT_MBUTTON_UP, button, .mbutton = { x, y } });

    return 0;
}


void add_input_escape_sequence(char *sequence, int keycode)
{
    int hash = hash_eseq(sequence);
    struct eseq_trans_list **etlp;

    for (etlp = &eseq_trans_list[hash]; *etlp != NULL; etlp = &(*etlp)->next)
    {
        if (!strcmp((*etlp)->sequence, sequence))
        {
            free(sequence);
            (*etlp)->keycode = keycode;
            return;
        }
    }

    struct eseq_trans_list *etl = malloc(sizeof(*etl));
    etl->next = NULL;
    etl->sequence = sequence;
    etl->keycode = keycode;

    *etlp = etl;
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
    else if ((inp >= 1) && (inp <= 26) && (inp != '\n'))
        inp = (inp + 96) | KEY_CONTROL;


    return inp;
}


void sim_input(int val)
{
    if (fifo_content + 1 > fifo_size)
        fifo = realloc(fifo, fifo_size + 16 * sizeof(fifo[0]));

    fifo[fifo_content++] = val;
}
