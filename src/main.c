#include <stdio.h>

#include "buffer.h"
#include "config.h"
#include "editor.h"
#include "term.h"


int main(int argc, char *argv[])
{
    load_config();

    term_init();


    if (argc < 2)
        new_buffer();
    else
    {
        for (int i = 1; i < argc; i++)
        {
            buffer_t *buf = new_buffer();

            if (!buffer_load(buf, argv[i]))
            {
                fprintf(stderr, "Could not load “%s”.\n", argv[i]);
                return 1;
            }
        }
    }


    editor();

    return 0;
}
