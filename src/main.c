#include <stdio.h>

#include "buffer.h"
#include "editor.h"
#include "term.h"


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Exactly one argument expected.\n");
        return 1;
    }


    term_init();


    if (!load_buffer(argv[1]))
    {
        fprintf(stderr, "Could not load “%s”.\n", argv[1]);
        return 1;
    }


    editor();

    return 0;
}
