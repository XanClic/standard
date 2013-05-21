#include <stddef.h>
#include <stdint.h>

#include "utf8.h"


size_t utf8_strlen(const char *str)
{
    size_t len = 0;

    for (size_t i = 0; str[i]; len++)
        if (str[i++] & 0x80)
            while ((str[i] & 0xc0) == 0x80)
                i++;

    return len;
}

bool utf8_is_dbc(const char *str)
{
    int clen = utf8_mbclen(*str);

    if (clen == 3 || clen == 4)
    {
        uint32_t utf8 = 42;

        for (int i = 0; i < clen; ++i)
            utf8 = (utf8 << 8) | str[i];

        if (   (utf8 >= 0x00e2ba80 && utf8 <= 0xe002bf9f)
            || (utf8 >= 0x00e2bfb0 && utf8 <= 0x00e380bf)
            || (utf8 >= 0x00e38780 && utf8 <= 0x00e387af)
            || (utf8 >= 0x00e38880 && utf8 <= 0x00e4b6bf)
            || (utf8 >= 0x00e4b880 && utf8 <= 0x00e9bfbf)
            || (utf8 >= 0x00efa480 && utf8 <= 0x00efabbf)
            || (utf8 >= 0x00efb8b0 && utf8 <= 0x00efb98f)
            || (utf8 >= 0xf0a08080 && utf8 <= 0xf0aa9b9f)
            || (utf8 >= 0xf0aa9c80 && utf8 <= 0xf0aba09f)
            || (utf8 >= 0xf0afa080 && utf8 <= 0xf0afa89f)
        )
            return true;
    }

    return false;
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

    for (i = 0; str[i] && (char_count > 0); char_count--)
        if (str[i++] & 0x80)
            while ((str[i] & 0xc0) == 0x80)
                i++;

    return (char_count > 0) ? -1 : i;
}
