#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "editor.h"
#include "term.h"
#include "tools.h"
#include "utf8.h"


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

    char *content = malloc(fsz + 1);
    fread(content, 1, fsz, fp);
    content[fsz] = 0;

    fclose(fp);


    buffer_t *buf = malloc(sizeof(*buf));

    buf->location = strdup(file);
    buf->x = buf->y = buf->ys = 0;
    buf->modified = false;

    buf->line_count = 1;
    for (int i = 0; content[i]; i++)
        if (content[i] == '\n')
            buf->line_count++;

    buf->linenr_width = get_decimal_length(buf->line_count);

    buf->lines = malloc(buf->line_count * sizeof(*buf->lines));
    buf->line_screen_pos = malloc(buf->line_count * sizeof(*buf->line_screen_pos));

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
        char *slash = strchr(&buf->location[i], '/');
        if (slash)
        {
            buf->name[oi++] = buf->location[i];
            if ((i = slash - buf->location))
                buf->name[oi++] = '/';
        }
        else
        {
            strcpy(&buf->name[oi], &buf->location[i]);
            break;
        }
    }


    buffer_list_append(buf);


    return true;
}


void buffer_destroy(buffer_t *buf)
{
    bool active_buffer_changed = false;

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
                if (blp == &buffer_list)
                    exit(0); // No active buffer remaining
                else
                {
                    // FIXME
                    active_buffer = *(buffer_t **)((uintptr_t)blp - offsetof(buffer_list_t, next) + offsetof(buffer_list_t, buffer));
                }
            }

            active_buffer_changed = true;
        }
    }


    free((char *)buf->location);
    free((char *)buf->name);

    for (int i = 0; i < buf->line_count; i++)
        free(buf->lines[i]);
    free(buf->lines);
    free(buf->line_screen_pos);

    free(buf);


    if (active_buffer_changed)
        update_active_buffer();
}


// Returns the byte offset in the current line (as opposed to active_buffer->x, which is the character offset)
static int get_active_line_byte_offset(void)
{
    int i, j;
    for (i = j = 0; j < active_buffer->x; i += utf8_mbclen(active_buffer->lines[active_buffer->y][i]), j++);
    return i;
}


static void ensure_line_size(char **lineptr, size_t len)
{
    *lineptr = realloc(*lineptr, len + 1);
}


void buffer_insert(buffer_t *buf, const char *string)
{
    buf->modified = true;

    int ofs = get_active_line_byte_offset();

    size_t line_len = strlen(active_buffer->lines[active_buffer->y]);

    const char *nl = strchr(string, '\n');
    if (!nl)
    {
        size_t str_len = strlen(string);

        ensure_line_size(&active_buffer->lines[active_buffer->y], line_len + str_len);
        memmove(&active_buffer->lines[active_buffer->y][ofs + str_len], &active_buffer->lines[active_buffer->y][ofs], line_len - ofs + 1);
        memcpy(&active_buffer->lines[active_buffer->y][ofs], string, str_len);

        active_buffer->x += utf8_strlen(string);

        return;
    }


    size_t str_len = nl - string;

    active_buffer->lines = realloc(active_buffer->lines, ++active_buffer->line_count * sizeof(*active_buffer->lines));

    memmove(&active_buffer->lines[active_buffer->y + 2], &active_buffer->lines[active_buffer->y + 1], (active_buffer->line_count - active_buffer->y - 2) * sizeof(*active_buffer->lines));

    active_buffer->lines[active_buffer->y + 1] = malloc(line_len - ofs + 1);

    strcpy(active_buffer->lines[active_buffer->y + 1], &active_buffer->lines[active_buffer->y][ofs]);

    ensure_line_size(&active_buffer->lines[active_buffer->y], ofs + str_len + 1);
    memcpy(&active_buffer->lines[active_buffer->y][ofs], string, str_len);
    active_buffer->lines[active_buffer->y][ofs + str_len] = 0;


    active_buffer->x = 0;
    active_buffer->y++;


    if (nl[1])
        buffer_insert(buf, nl + 1);
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


void buffer_activate_next(void)
{
    buffer_list_t *bl;
    for (bl = buffer_list; (bl != NULL) && (bl->buffer != active_buffer); bl = bl->next);

    if ((bl != NULL) && (bl->next != NULL))
        active_buffer = bl->next->buffer;
    else
        active_buffer = buffer_list->buffer;

    update_active_buffer();
}


void buffer_activate_prev(void)
{
    buffer_list_t *bl;
    for (bl = buffer_list; (bl->next != NULL) && (bl->next->buffer != active_buffer); bl = bl->next);

    active_buffer = bl->buffer;

    update_active_buffer();
}
