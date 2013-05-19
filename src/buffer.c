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


static void update_buffer_name(buffer_t *buf)
{
    free(buf->name);

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

    full_redraw();
}


buffer_t *new_buffer(void)
{
    buffer_t *buf = malloc(sizeof(*buf));

    buf->location = NULL;
    buf->x = buf->y = buf->ys = 0;
    buf->modified = false;

    buf->line_count = 1;
    buf->linenr_width = 1;

    buf->lines = malloc(sizeof(*buf->lines));
    buf->line_screen_pos = malloc(sizeof(*buf->line_screen_pos));

    buf->lines[0] = malloc(1);
    buf->lines[0][0] = 0;

    buf->name = strdup("[unnamed]");


    buffer_list_append(buf);

    return buf;
}


bool buffer_load(buffer_t *buf, const char *source)
{
    FILE *fp = fopen(source, "r");

    if (fp == NULL)
        return false;


    fseek(fp, 0, SEEK_END);
    size_t fsz = ftell(fp);
    rewind(fp);

    char *content = malloc(fsz + 1);
    fread(content, 1, fsz, fp);
    content[fsz] = 0;

    fclose(fp);


    free(buf->location);
    for (int i = 0; i < buf->line_count; i++)
        free(buf->lines[i]);
    free(buf->lines);
    free(buf->line_screen_pos);


    buf->location = strdup(source);

    buf->x = buf->y = buf->ys = 0;
    buf->modified = false;

    buf->line_count = 1;
    for (int i = 0; content[i]; i++)
        if ((content[i] == '\n') && content[i + 1]) // Don't count an empty line at EOF
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
        else if (!*content_pos) // Ignore empty line at EOF
            break;

        if (newline && (newline != content_pos) && (newline[-1] == '\r'))
            newline[-1] = 0;

        buf->lines[i] = strdup(content_pos);

        content_pos = newline ? (newline + 1) : NULL;
    }


    update_buffer_name(buf);


    return true;
}


bool buffer_write(buffer_t *buf, const char *target)
{
    const char *fname = target ? target : buf->location;

    if (!fname)
        return false;

    FILE *fp = fopen(fname, "w");
    if (fp == NULL)
        return false;

    for (int i = 0; i < buf->line_count; i++)
    {
        fputs(buf->lines[i], fp);
        fputc('\n', fp);
    }

    fclose(fp);

    if (target)
    {
        free(buf->location);
        buf->location = strdup(target);

        update_buffer_name(buf);
    }

    if (buf->modified)
    {
        buf->modified = false;
        full_redraw();
    }

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


    free(buf->location);
    free(buf->name);

    for (int i = 0; i < buf->line_count; i++)
        free(buf->lines[i]);
    free(buf->lines);
    free(buf->line_screen_pos);

    free(buf);


    if (active_buffer_changed)
        update_active_buffer();
}


static void ensure_line_size(char **lineptr, size_t len)
{
    *lineptr = realloc(*lineptr, len + 1);
}


void buffer_insert(buffer_t *buf, const char *string)
{
    buf->modified = true;

    int ofs = utf8_byte_offset(buf->lines[buf->y], buf->x);

    size_t line_len = strlen(buf->lines[buf->y]);

    const char *nl = strchr(string, '\n');
    if (!nl)
    {
        size_t str_len = strlen(string);

        ensure_line_size(&buf->lines[buf->y], line_len + str_len);
        memmove(&buf->lines[buf->y][ofs + str_len], &buf->lines[buf->y][ofs], line_len - ofs + 1);
        memcpy(&buf->lines[buf->y][ofs], string, str_len);

        buf->x += utf8_strlen(string);

        return;
    }


    size_t str_len = nl - string;

    buf->lines = realloc(buf->lines, ++buf->line_count * sizeof(*buf->lines));

    memmove(&buf->lines[buf->y + 2], &buf->lines[buf->y + 1], (buf->line_count - buf->y - 2) * sizeof(*buf->lines));

    buf->lines[buf->y + 1] = malloc(line_len - ofs + 1);

    strcpy(buf->lines[buf->y + 1], &buf->lines[buf->y][ofs]);

    ensure_line_size(&buf->lines[buf->y], ofs + str_len + 1);
    memcpy(&buf->lines[buf->y][ofs], string, str_len);
    buf->lines[buf->y][ofs + str_len] = 0;


    buf->x = 0;
    buf->y++;


    if (nl[1])
        buffer_insert(buf, nl + 1);
}


void buffer_delete(buffer_t *buf, int char_count)
{
    while (char_count > 0)
    {
        int remaining = utf8_strlen(buf->lines[buf->y]) - buf->x;
        int x_offset = utf8_byte_offset(buf->lines[buf->y], buf->x);


        if (remaining == char_count)
        {
            if (buf->x > 0)
                buf->x -= utf8_mbclen(buf->lines[buf->x][x_offset]);

            buf->lines[buf->y][x_offset] = 0;
        }
        else if (remaining > char_count)
        {
            int bytes = utf8_byte_offset(&buf->lines[buf->y][x_offset], char_count);
            memmove(&buf->lines[buf->y][x_offset], &buf->lines[buf->y][x_offset + bytes], strlen(&buf->lines[buf->y][x_offset + bytes]) + 1); // inkl. NUL
        }
        else
        {
            if (buf->line_count <= buf->y + 1)
            {
                if (buf->x > 0)
                    buf->x -= utf8_mbclen(buf->lines[buf->x][x_offset]);

                buf->lines[buf->y][x_offset] = 0;
                return;
            }


            // TODO: Optimize (multiple lines at once, if necessary)
            size_t next_line_length = strlen(buf->lines[buf->y + 1]);

            buf->lines[buf->y] = realloc(buf->lines[buf->y], x_offset + next_line_length + 1);
            memmove(&buf->lines[buf->y][x_offset], buf->lines[buf->y + 1], next_line_length + 1);

            free(buf->lines[buf->y + 1]);
            memmove(&buf->lines[buf->y + 1], &buf->lines[buf->y + 2], (--buf->line_count - buf->y - 1) * sizeof(buf->lines[0]));

            remaining++; // newline
        }


        char_count -= remaining;

        buf->modified = true;
    }
}


void buffer_list_append(buffer_t *buf)
{
    buffer_list_t *bl = malloc(sizeof(*bl));

    bl->buffer = buf;

    if (buffer_list == NULL)
    {
        bl->next = NULL;
        buffer_list = bl;
    }
    else
    {
        buffer_list_t *blp;
        for (blp = buffer_list; (blp->next != NULL) && (blp->buffer != active_buffer); blp = blp->next);

        bl->next = blp->next;
        blp->next = bl;
    }


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
