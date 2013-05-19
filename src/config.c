#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/data.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include "buffer.h"
#include "config.h"
#include "editor.h"
#include "events.h"
#include "input.h"
#include "keycodes.h"
#include "syntax.h"
#include "term.h"


int tabstop_width = 8;

extern color_t syntax_fg[], syntax_bg[];
extern bool syntax_underline[], syntax_bold[];


static mrb_state *gmrbs;


static const char *syntax_names[SYNREG_COUNT] = {
    "default",
    "error",
    "tabbar",
    "tab_active_inner",
    "tab_active_outer",
    "tab_inactive_inner",
    "tab_inactive_outer",
    "linenr",
    "placeholder_empty",
    "placeholder_line",
    "statusbar",
    "modebar"
};

static const struct
{
    const char *name;
    int value;
} key_aliases[] = {
    { "BS",         KEY_BACKSPACE },
    { "DEL",        KEY_DELETE },
    { "INS",        KEY_INSERT },
    { "HOME",       KEY_HOME },
    { "END",        KEY_END },
    { "UP",         KEY_UP },
    { "DOWN",       KEY_DOWN },
    { "LEFT",       KEY_LEFT },
    { "RIGHT",      KEY_RIGHT },
    { "PAGEUP",     KEY_PGUP },
    { "PAGEDOWN",   KEY_PGDOWN },
    { "F1",         KEY_F1 },
    { "F2",         KEY_F2 },
    { "F3",         KEY_F3 },
    { "F4",         KEY_F4 },
    { "F5",         KEY_F5 },
    { "F6",         KEY_F6 },
    { "F7",         KEY_F7 },
    { "F8",         KEY_F8 },
    { "F9",         KEY_F9 },
    { "F10",        KEY_F10 },
    { "F11",        KEY_F11 },
    { "F12",        KEY_F12 },
};


static void unhandled_exception(const char *file, mrb_state *mrbs)
{
    term_release();


    mrb_value msg = mrb_funcall(mrbs, mrb_obj_value(mrbs->exc), "message", 0);
    fprintf(stderr, "Unhandled exception in %s: %s\n", file, mrb_string_value_cstr(mrbs, &msg));

    fprintf(stderr, "Backtrace:\n");

    // Taken from mruby itself
    mrb_int ciidx = mrb_fixnum(mrb_obj_iv_get(mrbs, mrbs->exc, mrb_intern(mrbs, "ciidx")));
    if (ciidx >= mrbs->ciend - mrbs->cibase)
        ciidx = 10; /* ciidx is broken... */

    for (int i = ciidx; i >= 0; i--)
    {
        mrb_callinfo *ci = &mrbs->cibase[i];

        if (MRB_PROC_CFUNC_P(ci->proc))
            continue;

        char sep;
        if (ci->target_class == ci->proc->target_class)
            sep = '.';
        else
            sep = '#';

        const char *method = mrb_sym2name(mrbs, ci->mid);
        if (method)
        {
            const char *cn = mrb_class_name(mrbs, ci->proc->target_class);

            if (cn)
                fprintf(stderr, "    in %s%c%s\n", cn, sep, method);
            else
                fprintf(stderr, "    in %s\n", method);
        }
        else
            fprintf(stderr, "    (unknown)\n");
    }


    exit(1);
}


static bool event_call(const event_t *event, void *info)
{
    (void)event;

    mrb_sym symbol = (mrb_sym)(uintptr_t)info;

    // TODO: Allow class/object methods
    mrb_funcall_argv(gmrbs, mrb_nil_value(), symbol, 0, NULL);

    if (gmrbs->exc != NULL)
        unhandled_exception("?", gmrbs);


    return true;
}


static mrb_value highlight(mrb_state *mrbs, mrb_value self)
{
    (void)self;

    mrb_value wat;
    mrb_get_args(mrbs, "A", &wat);

    if (mrb_ary_len(mrbs, wat) != 2)
        mrb_raise(mrbs, mrbs->object_class, "Two arguments expected for “highlight”");


    mrb_value num  = mrb_ary_shift(mrbs, wat);
    mrb_value opts = mrb_ary_shift(mrbs, wat);

    if (!mrb_fixnum_p(num))
        mrb_raise(mrbs, mrbs->object_class, "Fixnum expected as first “highlight” argument");

    if (!mrb_hash_p(opts))
        mrb_raise(mrbs, mrbs->object_class, "Option hash expected as second “highlight” argument");


    int i = mrb_fixnum(num);
    if ((i < 0) || (i >= SYNREG_COUNT))
        mrb_raisef(mrbs, mrbs->object_class, "Highlight region %i is out of range", i);


    syntax_underline[i] = mrb_bool(mrb_hash_get(mrbs, opts, mrb_check_intern_cstr(mrbs, "underline")));
    syntax_bold[i] = mrb_bool(mrb_hash_get(mrbs, opts, mrb_check_intern_cstr(mrbs, "bold")));

    mrb_value fg = mrb_hash_get(mrbs, opts, mrb_check_intern_cstr(mrbs, "termfg"));
    mrb_value bg = mrb_hash_get(mrbs, opts, mrb_check_intern_cstr(mrbs, "termbg"));


    if (mrb_nil_p(fg))
        syntax_fg[i].type = COL_DEFAULT;
    else
    {
        int val = mrb_fixnum(fg); // FIXME
        syntax_fg[i].type = (val < 8) ? COL_8 : (val < 16) ? COL_16 : COL_256;
        syntax_fg[i].color = val;
    }


    if (mrb_nil_p(bg))
        syntax_bg[i].type = COL_DEFAULT;
    else
    {
        int val = mrb_fixnum(bg); // FIXME
        syntax_bg[i].type = (val < 8) ? COL_8 : (val < 16) ? COL_16 : COL_256;
        syntax_bg[i].color = val;
    }


    return mrb_nil_value();
}


static mrb_value imapf(mrb_state *mrbs, mrb_value self)
{
    (void)self;

    mrb_value mappings;
    mrb_get_args(mrbs, "H", &mappings);


    mrb_value keys = mrb_hash_keys(mrbs, mappings);

    mrb_value key;
    while (!mrb_nil_p(key = mrb_ary_shift(mrbs, keys)))
    {
        if (!mrb_fixnum_p(key))
        {
            // TODO: One-character strings
            mrb_raise(mrbs, mrbs->object_class, "Integer expected for imapf");
        }

        mrb_value fname = mrb_hash_get(mrbs, mappings, key);
        if (!mrb_symbol_p(fname))
            mrb_raise(mrbs, mrbs->object_class, "imapf must map onto a symbol");

        register_event_handler((event_t){ EVENT_INSERT_KEY, mrb_fixnum(key) }, event_call, (void *)(uintptr_t)mrb_symbol(fname));
    }


    return mrb_nil_value();
}


static mrb_value input(mrb_state *mrbs, mrb_value self)
{
    (void)self;

    int argc;
    mrb_value *argv;

    mrb_get_args(mrbs, "*", &argv, &argc);


    for (int i = 0; i < argc; i++)
    {
        if (!mrb_fixnum_p(argv[i]))
            mrb_raise(mrbs, mrbs->object_class, "Integer expected for input");

        sim_input(mrb_fixnum(argv[i]));
    }


    return mrb_nil_value();
}


static void nop_free(mrb_state *mrbs, void *ptr)
{
    (void)mrbs;
    (void)ptr;
}

static struct mrb_data_type buf_type = { "Buffer", nop_free };
static struct mrb_data_type buflin_type = { "BufferLines", nop_free };

static struct RClass *bufcls, *buflincls;

static mrb_value get_active_buffer(mrb_state *mrbs, mrb_value self)
{
    (void)self;
    return mrb_obj_value(mrb_data_object_alloc(mrbs, bufcls, active_buffer, &buf_type));
}

static mrb_value buffer_get_x(mrb_state *mrbs, mrb_value self)
{
    (void)mrbs;
    return mrb_fixnum_value(((buffer_t *)DATA_PTR(self))->x);
}

static mrb_value buffer_set_x(mrb_state *mrbs, mrb_value self)
{
    mrb_int value;
    mrb_get_args(mrbs, "i", &value);
    return mrb_fixnum_value(((buffer_t *)DATA_PTR(self))->x = value);
}

static mrb_value buffer_get_y(mrb_state *mrbs, mrb_value self)
{
    (void)mrbs;
    return mrb_fixnum_value(((buffer_t *)DATA_PTR(self))->y);
}

static mrb_value buffer_set_y(mrb_state *mrbs, mrb_value self)
{
    mrb_int value;
    mrb_get_args(mrbs, "i", &value);
    return mrb_fixnum_value(((buffer_t *)DATA_PTR(self))->y = value);
}

static mrb_value buffer_get_lines(mrb_state *mrbs, mrb_value self)
{
    return mrb_obj_value(mrb_data_object_alloc(mrbs, buflincls, DATA_PTR(self), &buflin_type));
}

static mrb_value buffer_del(mrb_state *mrbs, mrb_value self)
{
    mrb_int chars;
    mrb_get_args(mrbs, "i", &chars);

    if (DATA_PTR(self) == active_buffer)
        delete_chars(chars);
    else
        buffer_delete(DATA_PTR(self), chars);

    return mrb_nil_value();
}

static mrb_value buffer_get_line(mrb_state *mrbs, mrb_value self)
{
    mrb_int line;
    mrb_get_args(mrbs, "i", &line);
    return mrb_str_new_cstr(mrbs, ((buffer_t *)DATA_PTR(self))->lines[line]);
}


void load_config(void)
{
    FILE *fp = fopen(".stdrc", "r");

    if (fp == NULL)
    {
        perror("Could not open .stdrc");
        exit(1);
    }


    gmrbs = mrb_open();


    for (int i = 0; i < SYNREG_COUNT; i++)
    {
        char definition[128];
        sprintf(definition, "def %s *opts\n[%i, opts.reduce(:merge)]\nend\n", syntax_names[i], i);
        mrb_load_string(gmrbs, definition);
    }

    mrb_load_string(gmrbs, "def bold\n{bold: true}\nend\n");
    mrb_load_string(gmrbs, "def underline\n{underline: true}\nend\n");

    mrb_define_method(gmrbs, gmrbs->object_class, "highlight", &highlight, ARGS_REQ(1));
    mrb_define_alias(gmrbs, gmrbs->object_class, "hi", "highlight");


    mrb_define_method(gmrbs, gmrbs->object_class, "imapf", &imapf, ARGS_REQ(2));
    mrb_define_method(gmrbs, gmrbs->object_class, "input", &input, ARGS_ANY());
    mrb_define_alias(gmrbs, gmrbs->object_class, "i", "input");

    for (int i = 0; i < (int)(sizeof(key_aliases) / sizeof(key_aliases[0])); i++)
        mrb_define_global_const(gmrbs, key_aliases[i].name, mrb_fixnum_value(key_aliases[i].value));


    bufcls = mrb_define_class(gmrbs, "Buffer", NULL);
    mrb_define_class_method(gmrbs, bufcls, "active", &get_active_buffer, ARGS_NONE());
    mrb_define_method(gmrbs, bufcls, "x",  &buffer_get_x, ARGS_NONE());
    mrb_define_method(gmrbs, bufcls, "x=", &buffer_set_x, ARGS_REQ(1));
    mrb_define_method(gmrbs, bufcls, "y",  &buffer_get_y, ARGS_NONE());
    mrb_define_method(gmrbs, bufcls, "y=", &buffer_set_y, ARGS_REQ(1));
    mrb_define_method(gmrbs, bufcls, "lines", &buffer_get_lines, ARGS_NONE());
    mrb_define_method(gmrbs, bufcls, "delete", &buffer_del, ARGS_REQ(1));

    buflincls = mrb_define_class(gmrbs, "BufferLines", NULL);
    mrb_define_method(gmrbs, buflincls, "[]", &buffer_get_line, ARGS_REQ(1));


    mrb_load_file(gmrbs, fp);
    fclose(fp);


    mrb_value tabstop_val = mrb_gv_get(gmrbs, mrb_intern_cstr(gmrbs, "$tabstop"));

    if (!mrb_nil_p(tabstop_val))
        tabstop_width = mrb_fixnum(tabstop_val);

    if (gmrbs->exc != NULL)
        unhandled_exception(".stdrc", gmrbs);
}
