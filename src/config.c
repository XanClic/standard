#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/compile.h>
#include <mruby/hash.h>
#include <mruby/variable.h>

#include "config.h"
#include "syntax.h"
#include "term.h"


int tabstop_width = 8;

static mrb_state *mrbs;


extern color_t syntax_fg[], syntax_bg[];
extern bool syntax_underline[], syntax_bold[];


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
    "statusbar"
};


static mrb_value highlight(mrb_state *mrb, mrb_value self)
{
    (void)self;

    mrb_value wat;

    mrb_get_args(mrb, "A", &wat);

    if (mrb_ary_len(mrb, wat) != 2)
        mrb_raise(mrb, mrb->object_class, "Two arguments expected for “highlight”");

    mrb_value num  = mrb_ary_shift(mrb, wat);
    mrb_value opts = mrb_ary_shift(mrb, wat);

    if (!mrb_fixnum_p(num))
        mrb_raise(mrb, mrb->object_class, "Fixnum expected as first “highlight” argument");

    if (!mrb_hash_p(opts))
        mrb_raise(mrb, mrb->object_class, "Option hash expected as second “highlight” argument");

    int i = mrb_fixnum(num);
    if ((i < 0) || (i >= SYNREG_COUNT))
        mrb_raisef(mrb, mrb->object_class, "Highlight region %i is out of range", i);

    syntax_underline[i] = mrb_bool(mrb_hash_get(mrb, opts, mrb_check_intern_cstr(mrb, "underline")));
    syntax_bold[i] = mrb_bool(mrb_hash_get(mrb, opts, mrb_check_intern_cstr(mrb, "bold")));

    mrb_value fg = mrb_hash_get(mrb, opts, mrb_check_intern_cstr(mrb, "termfg"));
    mrb_value bg = mrb_hash_get(mrb, opts, mrb_check_intern_cstr(mrb, "termbg"));

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


void load_config(void)
{
    FILE *fp = fopen(".stdrc", "r");
    if (fp == NULL)
    {
        perror("Could not open .stdrc");
        exit(1);
    }

    mrbs = mrb_open();

    for (int i = 0; i < SYNREG_COUNT; i++)
    {
        char definition[128];
        sprintf(definition, "def %s *opts\n[%i, opts.reduce(:merge)]\nend\n", syntax_names[i], i);
        mrb_load_string(mrbs, definition);
    }

    mrb_load_string(mrbs, "def bold\n{bold: true}\nend\n");
    mrb_load_string(mrbs, "def underline\n{underline: true}\nend\n");

    mrb_define_method(mrbs, mrbs->object_class, "highlight", &highlight, ARGS_REQ(1));
    mrb_define_alias(mrbs, mrbs->object_class, "hi", "highlight");

    mrb_load_file(mrbs, fp);
    fclose(fp);

    mrb_value tabstop_val = mrb_gv_get(mrbs, mrb_intern_cstr(mrbs, "$tabstop"));

    if (!mrb_nil_p(tabstop_val))
        tabstop_width = mrb_fixnum(tabstop_val);
}
