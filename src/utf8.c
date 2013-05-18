#include <stddef.h>

#include "utf8.h"


size_t utf8_strlen(const char *str)
{
    size_t len = 0;

    for (size_t i = 0; str[i]; i++, len++)
        while ((str[i] & 0x80) && ((str[i] & 0xc0) != 0x80))
            i++;

    return len;
}


int utf8_mbclen(char start_chr)
{
    if (!(start_chr & 0x80))
        return 1;
    else if ((start_chr & 0xe0) == 0xc0)
        return 2;
    else if ((start_chr & 0xf0) == 0xe0)
        return 3;
    else if ((start_chr & 0xf8) == 0xf0)
        return 4;
    else
        return 1; // failsafe
}


int utf8_byte_offset(const char *str, int char_count)
{
    int i;

    for (i = 0; str[i] && (char_count > 0); i++, char_count--)
        while ((str[i] & 0x80) && ((str[i] & 0xc0) != 0x80))
            i++;

    return (char_count > 0) ? -1 : i;
}
