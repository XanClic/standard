#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "term.h"
#include "tools.h"


buffer_list_t *buffer_list = NULL;
buffer_t *active_buffer = NULL;


bool load_buffer(const char *file)
{
    FILE *fp = fopen(file, "r");

    if (fp == NULL)
        return false;


    fseek(fp, 0, SEEK_END);
    size_t fsz = ftell(fp);
    rewind(fp);

    char *content = malloc(fsz);
    fread(content, 1, fsz, fp);

    fclose(fp);


    buffer_t *buf = malloc(sizeof(*buf));

    buf->location = strdup(file);
    buf->x = buf->y = buf->ys = 0;

    buf->line_count = 0;
    for (int i = 0; content[i]; i++)
        if (content[i] == '\n')
            buf->line_count++;

    buf->linenr_width = get_decimal_length(buf->line_count);

    buf->lines = malloc(buf->line_count * sizeof(*buf->lines));

    char *content_pos = content;
    for (int i = 0; content_pos; i++)
    {
        char *newline = strchr(content_pos, '\n');
        if (newline)
            *newline = 0;

        if (newline && (newline != content_pos) && (newline[-1] == '\r'))
            newline[-1] = 0;

        buf->lines[i] = strdup(content_pos);

        content_pos = newline ? (newline + 1) : NULL;
    }


    buf->name = malloc(strlen(buf->location) + 1);
    for (int i = 0, oi = 0; buf->location[i]; i++)
    {
        if (strchr(&buf->location[i], '/'))
        {
            buf->name[oi++] = buf->location[i];
            if (i)
                buf->name[oi++] = '/';

            i = strchr(&buf->location[i], '/') - buf->location;
        }
        else
        {
            strcat(&buf->name[oi], &buf->location[i]);
            break;
        }
    }


    buffer_list_append(buf);


    return true;
}


void buffer_destroy(buffer_t *buf)
{
    buffer_list_t **blp;
    for (blp = &buffer_list; (*blp != NULL) && ((*blp)->buffer != buf); blp = &(*blp)->next);

    if (*blp != NULL)
    {
        buffer_list_t *blp_next = (*blp)->next;
        free(*blp);
        *blp = blp_next;

        if (active_buffer == buf)
        {
            if (blp_next != NULL)
                active_buffer = blp_next->buffer;
            else
            {
                // FIXME
                active_buffer = *(buffer_t **)((uintptr_t)blp - offsetof(buffer_list_t, next) + offsetof(buffer_list_t, buffer));
            }
        }
    }


    free((char *)buf->location);
    free((char *)buf->name);

    for (int i = 0; i < buf->line_count; i++)
        free(buf->lines[i]);
    free(buf->lines);

    free(buf);
}


void buffer_list_append(buffer_t *buf)
{
    buffer_list_t *bl = malloc(sizeof(*bl));

    bl->next = NULL;
    bl->buffer = buf;

    buffer_list_t **blp;
    for (blp = &buffer_list; *blp != NULL; blp = &(*blp)->next);

    *blp = bl;


    if (!active_buffer)
        active_buffer = buf;
}
