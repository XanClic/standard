#include <stdbool.h>

#include "syntax.h"
#include "term.h"


// Static allocation makes this default colors
color_t syntax_fg[SYNREG_COUNT], syntax_bg[SYNREG_COUNT];
// And this false
bool syntax_underline[SYNREG_COUNT], syntax_bold[SYNREG_COUNT];


void syntax_region(enum syntax_region region)
{
    term_set_color(syntax_fg[region], syntax_bg[region]);
    term_underline(syntax_underline[region]);
    term_bold(syntax_bold[region]);
}
