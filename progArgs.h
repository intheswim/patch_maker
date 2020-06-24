
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

struct progArguments
{
    char *file1;
    char *file2;
    char *file3;
    bool flagCreate;
    bool flagApply;
    bool flagForce;
    bool flagDiskIO;
    bool flagQuiet;
    bool flagVerbose;
    bool flagShowStats;
    bool flagIgnoreTimestamps;
    bool flagMaximizeCompression; // double number of refs.

    progArguments ()
    {
        file1 = file2 = file3 = nullptr;
        flagCreate = false;
        flagApply = false;
        flagForce = false;
        flagDiskIO = false;
        flagQuiet = false;
        flagVerbose = false;
        flagShowStats = false;
        flagIgnoreTimestamps = false;
        flagMaximizeCompression = false;
    }

    ~progArguments ()
    {
        free (file1);
        free (file2);
        free (file3);
    }

    int CreatePatchFileName ()
    {
        int i = 0;

        char szNewName [PATH_MAX] = { 0 };

        if (strlen(file2) > PATH_MAX - 1)
        {
            return -1;
        }

        strcpy (szNewName, file2);
        
        for (i = (int)strlen(file2) - 1; file2[i] != '.' && i > 0; i--);

        if (i == 0)  // name may no point and extention
            i = (int)strlen(file2);

        if (i > PATH_MAX - 5)
        {
            fprintf (stderr, "File name is too long\n");
            return -1;
        }

        strcpy (szNewName + i, ".foc");

        printf ("Patch file set to %s\n", szNewName);

        this->file3 = strdup (szNewName);

        return 0;
    }

    int parseArguments (int argc, char *argv[]);

};

