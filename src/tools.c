#include "tools.h"


int get_decimal_length(int number)
{
    int len = (number < 0);

    while (number)
    {
        len++;
        number /= 10;
    }

    return len;
}
