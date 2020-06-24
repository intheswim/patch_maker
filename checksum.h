#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// returns zero on success.
int getChecksum (FILE *fp, uint32_t & checksum);