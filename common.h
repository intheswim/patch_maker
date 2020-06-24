#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <io.h>
#define strdup _strdup
#define access _access
#define open  _open
#define close _close
#define F_OK  0
#define O_ACCMODE 0
#endif

#ifdef _MSC_VER
#include <../include/limits.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
#include <limits.h>
#endif

#define _VERSION_   1
#define BYTE_PATCH_EOF 8  // end of patch indicator.
#define BYTE_PATCH_SEQ 4  // sequence of identical bytes (e.g. AAAAAA).

struct AutoClose // RAII structure
{
    FILE *fp1;
    FILE *fp2;
    FILE *fp3;
    char *buffer1;
    char *buffer2;
    AutoClose () : fp1(0), fp2(0), fp3(0), buffer1(NULL), buffer2(NULL) {}
    ~AutoClose ()
    {
        if (fp1) { fclose (fp1); }
        if (fp2) { fclose (fp2); }
        if (fp3) { fclose (fp3); }
        if (buffer1) { free (buffer1); buffer1 = NULL; }
        if (buffer2) { free (buffer2); buffer2 = NULL; }
    }
};

// stats struct
struct PatchStats
{
    long refs_count ;
    long refs_length ;
    long refs_longest ;
    long as_is_length ;
    long as_is_count ;
    long as_is_maxed ;

    PatchStats () : refs_count(0), refs_length(0), refs_longest(0), 
                    as_is_length(0), as_is_count(0), as_is_maxed (0) { };
};

unsigned char getEndianness();
char * getEndiannessString (char *buffer, int len);

int createPatch (const struct progArguments & args);
int applyPatch (const struct progArguments & args);
