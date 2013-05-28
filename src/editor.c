#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "commands.h"
#include "config.h"
#include "editor.h"
#include "events.h"
#include "input.h"
#include "keycodes.h"
#include "syntax.h"
#include "term.h"
#include "tools.h"
#include "utf8.h"


static int desired_cursor_x = 0;

enum input_mode input_mode = MODE_NORMAL;


// Screen lines required
static int slr(buffer_t *buf, int line)
{
    return (1 + buf->linenr_width + 1 + utf8_strlen_vis(buf->lines[line]) + buffer_width - 1) / buffer_width;
}


void reposition_cursor(bool update_desire)
{
    term_cursor_pos(term_width - 16, term_height - 2);
    syntax_region(SYNREG_STATUSBAR);
    int position = printf("%i,%i", active_buffer->y + 1, active_buffer->x + 1);
    printf("%*c", 13 - position, ' ');


    static int old_x = -1, old_y = -1;

    if (old_x >= 0)
    {
        int old_line = -1, old_buf_i;

        // TODO: Optimize
        for (int line = active_buffer->ys + 1; line <= active_buffer->ye; line++)
        {
            if (active_buffer->line_screen_pos[line] > old_y)
            {
                old_line = line - 1;
                break;
            }
        }

        term_cursor_pos(old_x, old_y);

        if (old_line < 0)
        {
            if (old_y < active_buffer->line_screen_pos[active_buffer->ye] + slr(active_buffer, active_buffer->ye))
                old_line = active_buffer->ye;
            else
            {
                syntax_region(SYNREG_PLACEHOLDER_EMPTY);
                putchar(old_x ? ' ' : '~');
            }
        }

        if (old_line >= 0)
        {
            int old_in_line_x = old_x - 1 - active_buffer->linenr_width - 1 + (old_y - active_buffer->line_screen_pos[old_line]) * buffer_width;
            int x = 0;
            old_buf_i = 0;

            while ((x < old_in_line_x) && active_buffer->lines[old_line][old_buf_i])
            {
                if (active_buffer->lines[old_line][old_buf_i] != '\t')
                    x += utf8_is_dbc(&active_buffer->lines[old_line][old_buf_i]) ? 2 : 1;
                else
                    x += tabstop_width - x % tabstop_width;

                old_buf_i += utf8_mbclen(active_buffer->lines[old_line][old_buf_i]);
            }

            syntax_region(SYNREG_DEFAULT);
            if ((x < old_in_line_x) || !active_buffer->lines[old_line][old_buf_i] || (active_buffer->lines[old_line][old_buf_i] == '\t'))
                putchar(' ');
            else
                for (int i = 0; i < utf8_mbclen(active_buffer->lines[old_line][old_buf_i]); i++)
                    putchar(active_buffer->lines[old_line][old_buf_i + i]);
        }
    }


    int x = 0, i, j, dbc = 0;
    for (i = j = 0; j < active_buffer->x; i += utf8_mbclen(active_buffer->lines[active_buffer->y][i]), j++)
    {
         if (utf8_is_dbc(&(active_buffer->lines[active_buffer->y][i])))
         {
             dbc++;
         }

        if (active_buffer->lines[active_buffer->y][i] == '\t')
            x += tabstop_width - x % tabstop_width;
        else
            x++;
    }

    if (update_desire)
        desired_cursor_x = x + dbc;


    // Wait with this test until here, so that desired_cursor_x may be updated
    if ((active_buffer->y < active_buffer->ys) || (active_buffer->y > active_buffer->ye))
    {
        fflush(stdout);
        return;
    }


    x += 1 + active_buffer->linenr_width + 1 + dbc;

    int y = active_buffer->line_screen_pos[active_buffer->y] + x / buffer_width;

    x %= buffer_width;


    term_cursor_pos(old_x = x, old_y = y);

    syntax_region(SYNREG_DEFAULT);
    term_invert(true);
    if (!active_buffer->lines[active_buffer->y][i] || (active_buffer->lines[active_buffer->y][i] == '\t'))
        putchar(' ');
    else
        for (int k = 0; k < utf8_mbclen(active_buffer->lines[active_buffer->y][i]); k++)
            putchar(active_buffer->lines[active_buffer->y][i + k]);
    term_invert(false);

    term_cursor_pos(x, y);


    fflush(stdout);
}


void ensure_cursor_visibility(void)
{
    if (active_buffer->y < active_buffer->ys)
    {
        active_buffer->ys = active_buffer->y;
        full_redraw();
    }
    else if (active_buffer->y > active_buffer->ye)
    {
        int lines = 0;
        int ys = active_buffer->y;

        while (lines < buffer_height)
        {
            lines += slr(active_buffer, ys);
            ys--;
        }

        active_buffer->ys = ys + 1;
        full_redraw();
    }
}


void error(const char *format, ...)
{
    term_cursor_pos(0, term_height - 1);
    syntax_region(SYNREG_ERROR);

    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);

    reposition_cursor(false);
}


static char *cmdtok(char *s)
{
    static char *saved;

    if (s != NULL)
        saved = s;

    if (saved == NULL)
        return NULL;

    s = saved;

    while (!isspace(*saved) && *saved)
    {
        if (*saved == '\\')
        {
            if (!*(++saved))
                break;
        }
        if (*saved == '"')
        {
            saved++;
            while ((*saved != '"') && *saved)
                saved++;
            if (!*saved)
                break;
        }
        if (*saved == '\'')
        {
            saved++;
            while ((*saved != '\'') && *saved)
                saved++;
            if (!*saved)
                break;
        }
        saved++;
    }

    if (isspace(*saved))
    {
        *saved = 0;
        while (isspace(*++saved));
    }

    if (!*saved)
        saved = NULL;

    return s;
}


static void command_line(void)
{
    term_cursor_pos(0, term_height - 1);
    syntax_region(SYNREG_DEFAULT);
    printf("%-*c", term_width - 1, ':');
    term_cursor_pos(1, term_height - 1);

    term_show_cursor(true);
    fflush(stdout);

    char cmd[128];

    int i = 0;
    while ((i < 127) && ((cmd[i++] = input_read()) != '\n'))
    {
        if (((cmd[i - 1] == ':') && (i == 1)) || !cmd[i - 1])
            cmd[--i] = 0;
        else if (cmd[i - 1] == 127)
        {
            print_flushed("\b \b");

            cmd[--i] = 0;
            if (i)
                cmd[--i] = 0;
            else
                break;
        }
        else
        {
            putchar(cmd[i - 1]);
            fflush(stdout);
        }
    }

    term_show_cursor(false);
    fflush(stdout);

    if (i <= 0)
    {
        reposition_cursor(false);
        return;
    }

    cmd[--i] = 0;


    // FIXME
    char *cmd_line[32];

    cmd_line[0] = cmdtok(cmd);
    for (int j = 0; cmd_line[j]; j++)
        cmd_line[j + 1] = cmdtok(NULL);


    for (i = 0; command_handlers[i].cmd && strcmp(command_handlers[i].cmd, cmd_line[0]); i++);

    if (command_handlers[i].cmd)
        command_handlers[i].execute(cmd_line);
    else
    {
        term_cursor_pos(0, term_height - 1);
        syntax_region(SYNREG_ERROR);
        printf("Unknown command “%s”.", cmd_line[0]);
        fflush(stdout);
    }


    reposition_cursor(false);
}


static void line_change_update_x(void)
{
    if (desired_cursor_x == -1)
    {
        int len = (int)utf8_strlen(active_buffer->lines[active_buffer->y]);
        active_buffer->x = (input_mode == MODE_INSERT) ? len : (len ? (len - 1) : 0);
        return;
    }


    int x = 0, i = 0, j = 0;
    for (; active_buffer->lines[active_buffer->y][i] && (x < desired_cursor_x); i += utf8_mbclen(active_buffer->lines[active_buffer->y][i]), j++)
    {
        if (utf8_is_dbc(&(active_buffer->lines[active_buffer->y][i])))
            ++x;

        if (active_buffer->lines[active_buffer->y][i] == '\t')
            x += tabstop_width - x % tabstop_width;
        else
            x++;
    }

    if ((desired_cursor_x == x) || (input_mode == MODE_INSERT))
        active_buffer->x = j;
    else
        active_buffer->x = j ? (j - 1) : 0;
}


static void draw_line(buffer_t *buffer, int line)
{
    syntax_region(SYNREG_LINENR);
    printf(" %*i ", buffer->linenr_width, line);

    int x = 0;

    syntax_region(SYNREG_DEFAULT);
    for (int i = 0; active_buffer->lines[line][i]; i++)
    {
        if (buffer->lines[line][i] == '\t')
        {
            printf("%*c", tabstop_width - x % tabstop_width, ' ');
            x += tabstop_width - x % tabstop_width;
        }
        else
        {
            putchar(buffer->lines[line][i]);
            if ((buffer->lines[line][i] & 0xc0) != 0x80)
            {
                if (utf8_is_dbc(&(buffer->lines[line][i])))
                    x++;

                x++;
            }
        }
    }


    printf("%*s\n", buffer_width - (2 + buffer->linenr_width + x) % buffer_width, "");
}


void write_string(const char *s)
{
    ensure_cursor_visibility();


    bool old_modified = active_buffer->modified;

    int old_slr = slr(active_buffer, active_buffer->y);
    int old_lc = active_buffer->line_count;

    buffer_insert(active_buffer, s);

    int new_slr = slr(active_buffer, active_buffer->y);

    if (!old_modified || (old_lc != active_buffer->line_count) || (old_slr != new_slr))
    {
        full_redraw();

        while (active_buffer->y > active_buffer->ye)
        {
            int screen_lines_required = new_slr - active_buffer->oll_unused_lines;
            while (screen_lines_required > 0)
                screen_lines_required -= slr(active_buffer, active_buffer->ys++);
            full_redraw();
        }
    }
    else
    {
        term_cursor_pos(0, active_buffer->line_screen_pos[active_buffer->y]);
        draw_line(active_buffer, active_buffer->y);
    }

    reposition_cursor(true);
}


void delete_chars(int count)
{
    ensure_cursor_visibility();


    bool old_modified = active_buffer->modified;

    int old_slr = slr(active_buffer, active_buffer->y);
    int old_lc = active_buffer->line_count;

    buffer_delete(active_buffer, count);

    int new_slr = slr(active_buffer, active_buffer->y);

    if (!old_modified || (old_lc != active_buffer->line_count) || (old_slr != new_slr))
        full_redraw();
    else
    {
        term_cursor_pos(0, active_buffer->line_screen_pos[active_buffer->y]);
        draw_line(active_buffer, active_buffer->y);
    }

    reposition_cursor(true);
}


void scroll(int lines)
{
    if (lines < 0)
    {
        active_buffer->ys += lines;
        if (active_buffer->ys < 0)
            active_buffer->ys = 0;
    }
    else if (lines > 0)
    {
        active_buffer->ys += lines;
        if (active_buffer->ys >= active_buffer->line_count)
            active_buffer->ys = active_buffer->line_count - 1;
    }

    full_redraw();
}


void editor(void)
{
    int current_command[64] = { 0 }, cci = 0;


    full_redraw();


    for (;;)
    {
        int inp = input_read();

        if (!inp)
            continue;


        switch (input_mode)
        {
            case MODE_INSERT:
                if (trigger_event((event_t){ EVENT_INSERT_KEY, .code = inp }))
                    continue;
                break;

            case MODE_NORMAL:
            {
                if (cci < 62)
                    current_command[cci++] = inp;

                if ((cci >= 62) || (inp == '\033') || trigger_event((event_t){ EVENT_NORMAL_KEY_SEQ, .key_seq = current_command }))
                {
                    cci = 0;
                    memset(current_command, 0, sizeof(current_command));
                    continue;
                }
                break;
            }

            case MODE_REPLACE: break;
        }


        if (input_mode == MODE_NORMAL)
        {
            switch (inp)
            {
                case ':':
                    command_line();
                    continue;

                case 'a':
                    // Advancing is always possible, except for when the line is empty
                    if (active_buffer->lines[active_buffer->y][0])
                        active_buffer->x++;
                case 'i':
                    input_mode = MODE_INSERT;
                    term_cursor_pos(0, term_height - 1);
                    syntax_region(SYNREG_MODEBAR);
                    print("--- INSERT ---");
                    reposition_cursor(true);
                    ensure_cursor_visibility();
                    continue;
            }
        }
        else if (inp == '\033')
        {
            term_cursor_pos(0, term_height - 1);
            printf("%-*c", term_width - 2, ' ');

            if ((input_mode == MODE_INSERT) && (active_buffer->x > 0))
                active_buffer->x--;

            input_mode = MODE_NORMAL;

            reposition_cursor(desired_cursor_x >= 0);
        }
        else if ((input_mode == MODE_INSERT) && (inp > 0) && (inp < 256))
        {
            char full_mbc[5] = { inp };
            if (inp & 0x80)
            {
                int expected = utf8_mbclen(inp);

                if (expected)
                    for (int i = 1; i < expected; i++)
                        full_mbc[i] = input_read();
            }

            write_string(full_mbc);
        }


        bool recognized_command = true;

        switch (inp)
        {
            case KEY_NSHIFT | KEY_LEFT:
                if (active_buffer->x)
                    active_buffer->x--;
                reposition_cursor(true);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_RIGHT:
                if (active_buffer->x < (int)utf8_strlen(active_buffer->lines[active_buffer->y]) - (input_mode != MODE_INSERT))
                    active_buffer->x++;
                reposition_cursor(true);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_DOWN:
                if (active_buffer->y < active_buffer->line_count - 1)
                    active_buffer->y++;
                if (active_buffer->y > active_buffer->ye)
                {
                    int screen_lines_required = slr(active_buffer, active_buffer->y) - active_buffer->oll_unused_lines;
                    while (screen_lines_required > 0)
                        screen_lines_required -= slr(active_buffer, active_buffer->ys++);
                    full_redraw();
                }
                line_change_update_x();
                reposition_cursor(false);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_UP:
                if (active_buffer->y > 0)
                    active_buffer->y--;
                line_change_update_x();
                if (active_buffer->y < active_buffer->ys)
                {
                    active_buffer->ys = active_buffer->y;
                    full_redraw();
                }
                reposition_cursor(false);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_END:
                desired_cursor_x = -1;
                line_change_update_x();
                reposition_cursor(false);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_HOME:
                active_buffer->x = 0;
                reposition_cursor(true);
                ensure_cursor_visibility();
                break;

            case KEY_NSHIFT | KEY_DELETE:
                delete_chars(1);
                break;

            default:
                recognized_command = false;
        }


        if (recognized_command)
        {
            cci = 0;
            memset(current_command, 0, sizeof(current_command));
        }
    }
}


void full_redraw(void)
{
    term_clear();


    int position = 1;
    syntax_region(SYNREG_TABBAR);
    putchar(' ');

    for (buffer_list_t *bl = buffer_list; bl != NULL; bl = bl->next)
    {
        buffer_t *buf = bl->buffer;

        syntax_region((buf == active_buffer) ? SYNREG_TAB_ACTIVE_OUTER : SYNREG_TAB_INACTIVE_OUTER);
        putchar('/');
        syntax_region((buf == active_buffer) ? SYNREG_TAB_ACTIVE_INNER : SYNREG_TAB_INACTIVE_INNER);
        printf(" %s%s ", buf->modified ? "*" : "", buf->name);
        syntax_region((buf == active_buffer) ? SYNREG_TAB_ACTIVE_OUTER : SYNREG_TAB_INACTIVE_OUTER);
        putchar('\\');

        syntax_region(SYNREG_TABBAR);
        putchar(' ');

        position += 2 + buf->modified + utf8_strlen(buf->name) + 3;
    }


    int remaining = (16 * term_width - position) % term_width; // FIXME

    printf("%*s\n", remaining, "");


    int y_pos = 1, line;

    for (line = active_buffer->ys; line < active_buffer->line_count; line++)
    {
        int new_y_pos = y_pos + slr(active_buffer, line);

        if (new_y_pos > term_height - 2)
            break;

        draw_line(active_buffer, line);

        active_buffer->line_screen_pos[line] = y_pos;

        y_pos = new_y_pos;
    }

    if (line < active_buffer->line_count)
    {
        active_buffer->oll_unused_lines = term_height - 2 - y_pos;
        while (y_pos++ < term_height - 2)
        {
            syntax_region(SYNREG_LINENR);
            printf(" %*i ", active_buffer->linenr_width,line);
            syntax_region(SYNREG_PLACEHOLDER_LINE);
            puts("@");
        }
    }
    else
    {
        active_buffer->oll_unused_lines = 0;
        while (y_pos++ < term_height - 2)
        {
            syntax_region(SYNREG_LINENR);
            printf(" %*s ", active_buffer->linenr_width, "-");
            syntax_region(SYNREG_PLACEHOLDER_EMPTY);
            puts("~");
        }
    }

    active_buffer->ye = line - 1;


    syntax_region(SYNREG_STATUSBAR);

    printf("%-*s", term_width - 16, active_buffer->location ? active_buffer->location : "[unsaved]");
    position = printf("%i,%i", active_buffer->y + 1, active_buffer->x + 1);

    printf("%*c", 13 - position, ' ');

    bool top = !active_buffer->ys;
    bool bot = (line >= active_buffer->line_count - 1);

    if (top && bot)
        puts("All");
    else if (top)
        puts("Top");
    else if (bot)
        puts("Bot");
    else
        printf("%2i%%\n", (active_buffer->ys * 100) / (active_buffer->line_count - line + active_buffer->ys));


    if (input_mode != MODE_NORMAL)
    {
        syntax_region(SYNREG_MODEBAR);
        if (input_mode == MODE_INSERT)
            print("--- INSERT ---");
        else if (input_mode == MODE_REPLACE)
            print("--- REPLACE ---");
    }


    reposition_cursor(false);
}


void update_active_buffer(void)
{
    full_redraw();

    desired_cursor_x = active_buffer->x;
}
