#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "keycodes.h"
#include "terminfo.h"


#define read8(fp) fgetc(fp)

static uint16_t read16(FILE *fp)
{
    uint16_t lo = read8(fp), hi = read8(fp);
    return lo | (hi << 8);
}


static char *readstr(FILE *fp, int str_sec_ofs, int str_tbl_ofs, int index)
{
    fseek(fp, str_sec_ofs + index * 2, SEEK_SET);

    uint16_t str_ofs = read16(fp);
    if (str_ofs == 0xffff)
        return NULL;

    fseek(fp, str_tbl_ofs + str_ofs, SEEK_SET);
    int length;
    for (length = 1; read8(fp); length++);
    fseek(fp, str_tbl_ofs + str_ofs, SEEK_SET);

    char *buf = malloc(length);
    fread(buf, 1, length, fp);

    return buf;
}


static const struct
{
    int terminfo_index;
    int keycode;
    const char *def_seq;
} special_codes[] = {
    {  11, KEY_NSHIFT | KEY_DOWN,       NULL        },
    {  12, KEY_NSHIFT | KEY_HOME,       NULL        },
    {  14, KEY_NSHIFT | KEY_LEFT,       NULL        },
    {  17, KEY_NSHIFT | KEY_RIGHT,      NULL        },
    {  19, KEY_NSHIFT | KEY_UP,         NULL        },
    {  55, KEY_NSHIFT | KEY_BACKSPACE,  NULL        },
    {  59, KEY_NSHIFT | KEY_DELETE,     "\033[3~"   },
    {  61, KEY_NSHIFT | KEY_DOWN,       "\033[B"    },
    {  66, KEY_NSHIFT | KEY_F1,         "\033OP"    },
    {  67, KEY_NSHIFT | KEY_F10,        "\033[21~"  },
    {  68, KEY_NSHIFT | KEY_F2,         "\033OQ"    },
    {  69, KEY_NSHIFT | KEY_F3,         "\033OR"    },
    {  70, KEY_NSHIFT | KEY_F4,         "\033OS"    },
    {  71, KEY_NSHIFT | KEY_F5,         "\033[15~"  },
    {  72, KEY_NSHIFT | KEY_F6,         "\033[17~"  },
    {  73, KEY_NSHIFT | KEY_F7,         "\033[18~"  },
    {  74, KEY_NSHIFT | KEY_F8,         "\033[19~"  },
    {  75, KEY_NSHIFT | KEY_F9,         "\033[20~"  },
    {  76, KEY_NSHIFT | KEY_HOME,       "\033[H"    },
    {  77, KEY_NSHIFT | KEY_INSERT,     "\033[2~"   },
    {  79, KEY_NSHIFT | KEY_LEFT,       "\033[D"    },
    {  81, KEY_NSHIFT | KEY_PGDOWN,     "\033[6~"   },
    {  82, KEY_NSHIFT | KEY_PGUP,       "\033[5~"   },
    {  83, KEY_NSHIFT | KEY_RIGHT,      "\033[C"    },
    {  87, KEY_NSHIFT | KEY_UP,         "\033[A"    },
    { 164, KEY_NSHIFT | KEY_END,        "\033[F"    },

    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_DOWN,         "\033[1;5B" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_LEFT,         "\033[1;5D" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_RIGHT,        "\033[1;5C" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_UP,           "\033[1;5A" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_PGDOWN,       "\033[6;5~" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_PGUP,         "\033[5;5~" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_HOME,         "\033[1;5H" },
    {  -1, KEY_NSHIFT | KEY_CONTROL | KEY_END,          "\033[1;5F" }
};


void terminfo_load(void)
{
    for (int i = 0; i < (int)(sizeof(special_codes) / sizeof(special_codes[0])); i++)
        if (special_codes[i].def_seq != NULL)
            add_input_escape_sequence(strdup(special_codes[i].def_seq), special_codes[i].keycode);

    const char *term_name = getenv("TERM");
    if (!term_name)
        return;

    char ti_fname[128];
    sprintf(ti_fname, "/usr/share/terminfo/%c/%s", term_name[0], term_name);

    FILE *ti_fp = fopen(ti_fname, "rb");
    if (!ti_fp)
        return;


    if (read16(ti_fp) != 0x011a)
    {
        fclose(ti_fp);
        return;
    }

    int names_sec_len = read16(ti_fp);
    int  bool_sec_len = read16(ti_fp);
    int   int_sec_len = read16(ti_fp);
    int   str_sec_len = read16(ti_fp);
    /*int str_tbl_len=*/read16(ti_fp);


    // Who cares for the terminal names or capabilities; it best hopes it does everything I want it to

    int str_sec_ofs = (ftell(ti_fp) + names_sec_len + bool_sec_len + int_sec_len * 2 + 1) & ~1;
    int str_tbl_ofs = str_sec_ofs + str_sec_len * 2;


    for (int i = 0; i < (int)(sizeof(special_codes) / sizeof(special_codes[0])); i++)
    {
        if (special_codes[i].terminfo_index < 0)
            continue;

        char *seq = readstr(ti_fp, str_sec_ofs, str_tbl_ofs, special_codes[i].terminfo_index);
        if (seq)
        {
            if ((seq[0] == 27) && (seq[1] == 'O'))
            {
                // Many terminfo files seem to contain \eO… when they're actually using \e[….
                // For some entries, this is not an issue, since there are both entries (e.g., cuu1 as \e[A and kcuu1 as \eOA for the up arrow).
                // For others, it is (e.g., there is home and khome, but just kend and no end). Therefore, if this entry is \eO…, also add the
                // \e[… variant.
                char *dup = strdup(seq);
                dup[1] = '[';
                add_input_escape_sequence(dup, special_codes[i].keycode);
            }

            add_input_escape_sequence(seq, special_codes[i].keycode);
        }
    }


    fclose(ti_fp);
}
