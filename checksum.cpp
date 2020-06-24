/* Simple public domain implementation of the standard CRC32 checksum.
 * Outputs the checksum for each file given as a command line argument.
 * Invalid file names and files that cause errors are silently skipped.
 * The program reads from stdin if it is called with no arguments. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "checksum.h"

static uint32_t crc32_for_byte(uint32_t r) 
{
  for(int j = 0; j < 8; ++j)
    r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;

  return r ^ (uint32_t)0xFF000000L;
}

static void crc32(const void *data, size_t n_bytes, uint32_t* crc, uint32_t * table) 
{
    for (size_t i = 0; i < n_bytes; ++i)
        *crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
}

////////////////////////////////////////////////////////////////////////////
// returns zero on success.
////////////////////////////////////////////////////////////////////////////

int getChecksum (FILE *fp, uint32_t & checksum)
{
    uint32_t table [256];

    for(uint32_t i = 0; i < 256; ++i)
      table[i] = crc32_for_byte(i);

    fseek (fp, 0, SEEK_SET);

    char buffer[256];

    uint32_t crc = 0;

    while (!feof(fp) && !ferror(fp))
    {
        size_t len = fread(buffer, 1, sizeof(buffer), fp);

        crc32(buffer, len, &crc, table);
    }

    checksum = crc;

    return (0 == ferror(fp)) ? 0 : -1;
}
