#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "commands.h"
#include "editor.h"
#include "syntax.h"
#include "term.h"


static void quit(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    if (!active_buffer->modified)
        buffer_destroy(active_buffer);
    else
        error("%s has been modified.", active_buffer->name);
}


static void force_quit(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    buffer_destroy(active_buffer);
}


static void quit_all(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    for (buffer_list_t *bl = buffer_list; bl != NULL; bl = bl->next)
    {
        if (bl->buffer->modified)
        {
            error("%s has been modified.", bl->buffer->name);
            return;
        }
    }

    exit(0);
}


static void force_quit_all(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    exit(0);
}


static void tabnext(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    buffer_activate_next();
}


static void tabprevious(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    buffer_activate_prev();
}


static bool raw_write(char **cmd_line)
{
    int parcount;
    for (parcount = 0; cmd_line[parcount]; parcount++);

    // The first parameter is the file to be associated, the following are just
    // duplicates – thus, write in backwards.
    for (int i = parcount - 1; i > 0; i--)
    {
        if (!buffer_write(active_buffer, cmd_line[i]))
        {
            error("Could not write to “%s”.", cmd_line[i]);
            return false;
        }
    }

    if (parcount <= 1)
    {
        if (!active_buffer->location)
        {
            error("No file associated with %s.", active_buffer->name);
            return false;
        }
        else if (!buffer_write(active_buffer, NULL))
        {
            error("Could not write to “%s”.", active_buffer->location);
            return false;
        }
    }

    return true;
}


static void buf_write(char **cmd_line)
{
    raw_write(cmd_line);
}


static void write_and_quit(char **cmd_line)
{
    if ((!cmd_line[1] && !active_buffer->modified) || raw_write(cmd_line))
        buffer_destroy(active_buffer);
}


static void write_and_quit_all(char **cmd_line)
{
    if (cmd_line[1])
    {
        error("Unexpected parameter.");
        return;
    }

    for (buffer_list_t *bl = buffer_list; bl != NULL; bl = bl->next)
    {
        if (bl->buffer->modified)
        {
            if (!bl->buffer->location)
            {
                error("No file associated with %s.", bl->buffer->name);
                return;
            }
            else if (!buffer_write(bl->buffer, NULL))
            {
                error("Could not write to “%s”.", bl->buffer->location);
                return;
            }
        }
    }

    exit(0);
}


struct cmd_handler command_handlers[] = {
    { "q", quit },
    { "q!", force_quit },
    { "qa", quit_all },
    { "qa!", force_quit_all },
    { "tabnext", tabnext },
    { "tabprevious", tabprevious },
    { "w", buf_write },
    { "x", write_and_quit },
    { "wq", write_and_quit },
    { "xa", write_and_quit_all },
    { "wqa", write_and_quit_all },

    { NULL, NULL }
};
