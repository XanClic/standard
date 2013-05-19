#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "buffer.h"
#include "config.h"
#include "editor.h"
#include "term.h"


static struct option long_options[] =
{
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
};

int main(int argc, char *argv[])
{
    int c;
    int option_index = 0;

    if (argc < 2)
    {
        fprintf(stderr, "standard: Argument expected.\n");
        return 1;
    }

    while ((c = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case 'h':
                printf("Usage: std FILE\n"
                       "--help, -h     Show this help\n"
                       "--version, -v  Show current version\n");
                return 0;
            case 'v':
                fprintf(stderr, "standard: the standard text editor.\n"
                        "No need for ed anymore.\n");
                return 0;
            default:
                return 0;
        }
    }
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
                term_release();

                fprintf(stderr, "Could not load “%s”.\n", argv[i]);
                return 1;
            }
        }
    }


    editor();

    return 0;
}
