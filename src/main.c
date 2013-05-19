#include <stdio.h>

#include "buffer.h"
#include "config.h"
#include "editor.h"
#include "term.h"


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Argument expected.\n");
        return 1;
    }


    load_config();

    term_init();


    for (int i = 1; i < argc; i++)
    {
        if (!load_buffer(argv[i]))
        {
            fprintf(stderr, "Could not load “%s”.\n", argv[i]);
            return 1;
        }
    }


    editor();

    return 0;
}
