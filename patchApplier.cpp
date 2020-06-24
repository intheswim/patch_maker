/* PatchMaker utility, Copyright (c) 1997-2020 Yuriy Yakimenko
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <assert.h>

#include <limits.h>

#include "common.h"
#include "progArgs.h"

#include "timeutil.h"
#include "checksum.h"

#define PAT_HEADER_SIZE 32

static int applyPatchBody (AutoClose & ac, const long blockSize, const progArguments & args)
{
    long pos = 0;

    bool rwError = false;

    FILE * fp1 = ac.fp1;
    FILE * fp2 = ac.fp2;
    FILE * fp3 = ac.fp3;

    uint8_t bLen = 0;
    uint16_t iLen = 0, iStart = 0;
    uint32_t lStart = 0;
    int32_t maxL = 0;

    unsigned char szBuff [256];

    bool eofFound = false;

    while (!rwError)
    {
        int byte = fgetc (fp3);
        
        if (byte == EOF) break;

        if (byte == 0)
        {
            if (fread (&bLen, 1, 1, fp3) != 1) { rwError = true; break; }
            iLen = bLen + 1;
            if (fread (szBuff, iLen, 1, fp3) != 1) { rwError = true; break; }
            if (fwrite (szBuff, iLen, 1, fp2) != 1) { rwError = true; break; }
            pos += iLen;
        }
        else if (byte == BYTE_PATCH_EOF) 
        {
            eofFound = true;
            break;
        }
        else if (byte == BYTE_PATCH_SEQ) // sequence of identical bytes.
        {
            if (fread (&bLen, 1, 1, fp3) != 1) { rwError = true; break; }
            iLen = bLen + 1;
            if (fread (&bLen, 1, 1, fp3) != 1) { rwError = true; break; }
            for (int i=0; i < iLen && !rwError; i++)
            if (fwrite (&bLen, 1, 1, fp2) != 1) { rwError = true; }
            pos += iLen;
        }
        else
        {
            if (fread (&iStart, 2, 1, fp3) != 1)
            {
                rwError = true;
                break;
            }

            lStart = byte >> 2;
            lStart = (lStart << 16) + iStart;
            byte &= 3;

            if (0 != fseek (fp1, lStart * blockSize, SEEK_SET))
            { 
                rwError = true;
                break;
            }

            if (byte == 1)
            {
                if (fread (&bLen, 1, 1, fp3) != 1) rwError = true;
                maxL = bLen;
            }
            else if (byte == 2)
            {
                if (fread (&iLen, 2, 1, fp3) != 1) rwError = true;
                maxL = iLen;
            }
            else if (byte == 3)
            {
                if (fread (&maxL, 4, 1, fp3) != 1) rwError = true;
            }
            else 
                break;

            pos += maxL;

            for ( ; maxL > 0 && !rwError; maxL -= 256)
            {
                iLen = maxL > 256 ? 256 : maxL;
                if (fread (szBuff, iLen, 1, fp1) != 1) rwError = true;
                if (fwrite (szBuff, iLen, 1, fp2) != 1) rwError = true;
            }
        }
    }

    return ((rwError == false) && eofFound) ? 0 : 1;
}

///////////////////////////////////////////////////////////////////////////
//
//  This is the only exposed function from this file.
//
///////////////////////////////////////////////////////////////////////////

int applyPatch (const struct progArguments & args)
{
    struct ftime actual_time1 = ftime(), // filling values with zeros
                time1 = ftime(), time2 = ftime();

    long actual_size1 = 0, actual_size2 = 0, patch_size = 0;

    uint32_t size1 = 0, size2 = 0;
    uint32_t checksum1 = 0, checksum2 = 0, actual_checksum1 = 0, actual_checksum2 = 0;

    if (sizeof(struct ftime) != 5)
    {
        fprintf(stderr, "Wrong time struct size. Cannot proceed.\n");
        return -1;
    }

    AutoClose ac;

    if (0 != getftime (args.file1, &actual_time1))
    {
        fprintf(stderr, "Could not open file %s\n", args.file1);
        return EXIT_FAILURE;
    }

    
    ac.fp1 = fopen (args.file1, "rb");

    if (ac.fp1 == NULL)
    {
        fprintf (stderr, "Could not open file %s\n", args.file1);
        return EXIT_FAILURE;
    }

    ac.fp3 = fopen (args.file3, "rb");

    if (ac.fp3 == NULL)
    {
        printf ("Could not open file %s.\n", args.file3);
        return EXIT_FAILURE;
    }

    fseek (ac.fp1, 0L, SEEK_END);
    actual_size1 = ftell (ac.fp1);
    fseek (ac.fp1, 0L, SEEK_SET);

    fseek (ac.fp3, 0L, SEEK_END);
    patch_size = ftell (ac.fp3);
    fseek (ac.fp3, 0L, SEEK_SET);

    if (patch_size < PAT_HEADER_SIZE)
    {
        fprintf (stderr, "Incomplete or corrupted patch header.\n");
        return EXIT_FAILURE;
    }

    unsigned char szCheck [6] = { 0 };

    if (fread (szCheck, 6, 1, ac.fp3) != 1)
    {
        fprintf (stderr, "Could not read patch header.\n");
        return EXIT_FAILURE;
    }

    if (memcmp (szCheck, "FOC", 3) != 0)
    {
        printf ("Not a patch file: %s\n", args.file3);
        return EXIT_FAILURE;
    }

    if (szCheck[3] != _VERSION_ )
    {
        fprintf (stderr, "Patch version %d not supported.\n", (int)szCheck[3]);
        return EXIT_FAILURE;
    }

    if (szCheck[5] == 0)
    {
        fprintf (stderr, "Incomplete or corrupted patch\n");
        return EXIT_FAILURE;
    }

    if (szCheck[5] != getEndianness())
    {
        fprintf (stderr, "Endinanness not supported.\n");
        return EXIT_FAILURE;
    }

    long blockSize = (long)szCheck [4] + 1;
    
    // we already know the file is at least as large as header, adding assert to fix warnings.

    bool headerError = false;
    if (fread(&time1, 5, 1, ac.fp3) != 1)
        headerError = true;

    if (fread(&time2, 5, 1, ac.fp3) != 1)
        headerError = true;

    if (fread(&size1, 4, 1, ac.fp3) != 1)
        headerError = true;

    if (fread(&size2, 4, 1, ac.fp3) != 1)
        headerError = true;

    if (fread(&checksum1, 4, 1, ac.fp3) != 1)
        headerError = true;

    if (fread(&checksum2, 4, 1, ac.fp3) != 1)
        headerError = true;

    if (headerError)
    {
        fprintf(stderr, "Error reading patch header\n");
        return EXIT_FAILURE;
    }

    if (size1 != actual_size1)
    {
        fprintf (stderr, "Original file size mismatch: %ld vs %ld\n", (long)size1, (long)actual_size1);
        return EXIT_FAILURE;
    }

    if (0 != getChecksum (ac.fp1, actual_checksum1))
    {
        fprintf (stderr, "Error validating %s.\n", args.file1);
        return EXIT_FAILURE;
    }

    if (actual_checksum1 != checksum1)
    {
        fprintf (stderr, "Original file checksum mismatch.\n");
        return EXIT_FAILURE;
    }

    if (memcmp (&actual_time1, &time1, sizeof(struct ftime)) != 0)
    {
        // file timestamp can be all zeros (ivalid value), 
        // in this case no warning is given.

        if (!args.flagIgnoreTimestamps && is_valid (&time1))
        {
            fprintf (stderr, "Original file time mismatch.\n");

            if (false == args.flagForce)
            {
                return EXIT_FAILURE;
            }
        }
    }

    if (access(args.file2, F_OK) == 0 && !args.flagForce)
    {
        printf("File %s already exists. Use -f flag to overwrite.\n", args.file2);
        return EXIT_SUCCESS;
    }

    ac.fp2 = fopen (args.file2, "w+b");

    if (ac.fp2 == NULL)
    {
        fprintf (stderr, "Could not open file %s.\n", args.file2);
        return EXIT_FAILURE;
    }

    int ret = applyPatchBody (ac, blockSize, args);

    if (ret != 0)
    {
        fprintf(stderr, "Read/write error\n");
    }
    else 
    {
        fseek (ac.fp2, 0L, SEEK_END);
        actual_size2 = ftell (ac.fp2);

        if (actual_size2 != (long)size2)
        {
            fprintf (stderr, "Output file size mismatch.\n");
            return EXIT_FAILURE;
        }

        getChecksum (ac.fp2, actual_checksum2);

        if (checksum2 != actual_checksum2)
        {
            fprintf (stderr, "Output file checksum mismatch.\n");
            return EXIT_FAILURE;
        }

        fclose (ac.fp2);
        ac.fp2 = nullptr;

        if (!args.flagIgnoreTimestamps && is_valid (&time2))
        {
            if (0 != setftime (args.file2, &time2))
            {
                fprintf (stderr, "Could not set output file modification timestamp\n");
            }
        }

        if (!args.flagQuiet)
        {
            printf ("File successfully built.\n");
        }
    }

    return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}