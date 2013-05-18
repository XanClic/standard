#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>


size_t utf8_strlen(const char *str);
int utf8_mbclen(char start_chr);
int utf8_byte_offset(const char *str, int char_count);

#endif
