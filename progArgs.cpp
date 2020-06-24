#include "progArgs.h"

const char szSyntax [] =
     "Syntax: ./patch [flags] oldfile.ext newfile.ext [file.pat]\n"
     "   flags: -c - create patch\n"
     "          -a - apply patch\n"
     "          -f - force overwrite\n"
     "          -i - ignore timestamps\n"
     "          -s - print stats\n"
     "          -q - quiet mode\n"
     "          -v - verbose mode\n"
     "          -d - disable I/O buffering\n"
     "          -x - maximize compression\n";

const char szCopyRight []= "Patch maker utility. (c) Yuriy Yakimenko. 1997-2020\n";


static void outputSyntax (bool showAuthor = false)
{
    if (showAuthor)
        printf ("%s", szCopyRight);

    printf ("%s", szSyntax);
}

int progArguments::parseArguments (int argc, char *argv[])
{
    bool fileNameSet = false;

    char combined_flags[32] = { 0 };

    if (argc == 1)
    {
        outputSyntax (true);
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strncmp (argv[i], "-", 1) == 0) // flags
        {
            if (fileNameSet)
            {
                outputSyntax ();
                return -1;
            }

            memset (combined_flags, 0, sizeof (combined_flags));

            strncpy (combined_flags, argv[i], sizeof (combined_flags) - 1);

            for (int j=1; combined_flags[j]; j++)
            {
                char flag = combined_flags[j];

                if (flag == 'c')
                {
                    flagCreate = true;
                }
                else if (flag == 'a')
                {
                    flagApply = true;
                }
                else if (flag == 'f')
                {
                    flagForce = true;
                }
                else if (flag == 'd')
                {
                    flagDiskIO = true;
                }
                else if (flag == 'q')
                {
                    flagQuiet = true;
                }
                else if (flag == 'v')
                {
                    flagVerbose = true;
                }
                else if (flag == 'i')
                {
                    flagIgnoreTimestamps = true;
                }
                else if (flag == 's')
                {
                    flagShowStats = true;
                }
                else if (flag == 'x')
                {
                    flagMaximizeCompression = true;
                }
                else 
                {
                    fprintf (stderr, "Unknown flag -%c\n", flag);
                    outputSyntax ();
                    return -1;
                }
            }
        }
        else // file name
        {
            fileNameSet = true;

            if (file1 == nullptr)
            {
                file1 = strdup (argv[i]);
            }
            else if (file2 == nullptr)
            {
                file2 = strdup (argv[i]);
            }
            else if (file3 == nullptr)
            {
                file3 = strdup (argv[i]);
            }
        }
    }

    if (flagQuiet && flagVerbose) // inconsistent args
    {
        fprintf (stderr, "Cannot combine -v and -q flags\n");
        return -1;
    }

    if (flagApply && flagCreate) // inconsistent args
    {
        fprintf (stderr, "Cannot combine -a and -c flags\n");
        return -1;
    }

    if (!flagApply && !flagCreate) // inconsistent args
    {
        fprintf (stderr, "-a and -c flags are missing\n");
        outputSyntax ();
        return -1;
    }

    if (file1 == nullptr || file2 == nullptr)
    {
        fprintf (stderr, "Input file names not set\n");
        return -1;
    }

    if (strcmp (file1, file2) == 0)
    {
        fprintf (stderr, "Two input files are the same\n");
        return -1;
    }

    if (file3 == nullptr)
    {
        if (0 != this->CreatePatchFileName())
        {
            return -1;
        }
    }

    return 0;
}
