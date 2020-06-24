#include <string.h>
#include "common.h"

static bool is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return (bint.c[0] == 1); 
}

unsigned char getEndianness()
{
    return is_big_endian() ? 2 : 1;
}

char * getEndiannessString (char *buffer, int len)
{
    if (is_big_endian())
        strncpy (buffer, "Big endian", len - 1);
    else 
        strncpy (buffer, "Little endian", len - 1);

    return buffer;
}