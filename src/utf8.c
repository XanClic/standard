#include <stddef.h>

#include "utf8.h"


size_t utf8_strlen(const char *str)
{
    size_t len = 0;

    for (size_t i = 0; str[i]; i++, len++)
        while ((str[i] & 0x80) && ((str[i] & 0xc0) != 0xc0))
            i++;

    return len;
}
