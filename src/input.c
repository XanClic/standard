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

static regex_t mouse_input_regex_1006, mouse_input_regex_1015;


void init_mouse_input_regex(void)
{
    regcomp(&mouse_input_regex_1006, "^\033\\[<\\([0-9]\\+\\);\\([0-9]\\+\\);\\([0-9]\\+\\)\\([mM]\\)$", 0);
    regcomp(&mouse_input_regex_1015, "^\033\\[\\([0-9]\\+\\);\\([0-9]\\+\\);\\([0-9]\\+\\)M$", 0);
}


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


    int match_type;

    regmatch_t matches[5];
    if (!regexec(&mouse_input_regex_1006, escape_sequence, 5, matches, 0))
        match_type = 1006;
    else if (!regexec(&mouse_input_regex_1015, escape_sequence, 4, matches, 0))
        match_type = 1015;
    else if ((escape_sequence[1] == '[') && (escape_sequence[2] == 'M'))
        match_type = 1000;
    else
        return 0;


    int button = (match_type == 1000) ? escape_sequence[3] : atoi(&escape_sequence[matches[1].rm_so]);
    int x      = (match_type == 1000) ? escape_sequence[4] : atoi(&escape_sequence[matches[2].rm_so]);
    int y      = (match_type == 1000) ? escape_sequence[5] : atoi(&escape_sequence[matches[3].rm_so]);

    int event_type = EVENT_MBUTTON_DOWN;

    if (match_type == 1006)
        event_type = (escape_sequence[matches[4].rm_so] == 'M') ? EVENT_MBUTTON_DOWN : EVENT_MBUTTON_UP;
    else if ((match_type == 1015) || (match_type == 1000))
    {
        button -= ' ';

        if (match_type == 1000)
        {
            x -= ' ';
            y -= ' ';
        }

        static int last_down = 0;

        if (button < 3)
            last_down = button;
        else if (button == 3)
        {
            button = last_down;
            event_type = EVENT_MBUTTON_UP;
        }
    }


    trigger_event((event_t){ event_type, button, .mbutton = { x, y } });

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
