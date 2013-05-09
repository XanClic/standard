#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>


size_t utf8_strlen(const char *str);
int utf8_mbclen(char start_chr);

#endif
