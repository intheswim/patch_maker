#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#else 
#include <stdint.h>
#endif

#include <fcntl.h>

#include <time.h>
#include <assert.h>
#include <memory.h>

#include "timeutil.h"
#include "common.h"

#include <stdio.h>

#ifndef _MSC_VER
#include <utime.h>
#include <sys/time.h>
#else 
#include <sys/utime.h>
#endif

// returns zero on success.

int getftime (const char *filename, struct ftime *ftimep, long *fsize)
{
    int32_t retValue = 0;
    struct stat buf;
    struct tm *ModTime;

    int handle = open (filename, O_ACCMODE);

    if (handle == -1) return -1;

    retValue = fstat (handle, &buf);

    close (handle);

    if (retValue)
      return (retValue);

    if (fsize)
    {
        *fsize = buf.st_size;
    }

    ModTime = localtime (&buf.st_mtime);

    ftimep->ft_tsec = ModTime->tm_sec ;
    ftimep->ft_min = ModTime->tm_min ;
    ftimep->ft_hour = ModTime->tm_hour;
    ftimep->ft_day = ModTime->tm_mday;
    ftimep->ft_month = ModTime->tm_mon + 1;
    ftimep->setYear (ModTime->tm_year + 1900);
    ftimep->ft_isdst = ModTime->tm_isdst;  

    return 0;
}

int setftime (const char *filename, const struct ftime * timep)
{
    struct tm ModTime;

    memset (&ModTime, 0, sizeof (struct tm));

    ModTime.tm_year = timep->getYear() - 1900;
    ModTime.tm_mon = timep->ft_month - 1;
    ModTime.tm_mday = timep->ft_day;
    ModTime.tm_hour = timep->ft_hour;
    ModTime.tm_min = timep->ft_min;
    ModTime.tm_sec = timep->ft_tsec;
    ModTime.tm_isdst = timep->ft_isdst; 

#ifdef _MSC_VER
    struct _utimbuf times;

    times.actime = mktime(&ModTime);
    times.modtime = mktime(&ModTime);

    int ret = _utime (filename, &times);

#else 
    int handle = open (filename, O_ACCMODE);

    if (handle == -1) return -1;

    struct timespec times2[2] ; 

    times2[0].tv_sec = mktime(&ModTime);
    times2[1].tv_sec = mktime(&ModTime);
    times2[0].tv_nsec = 0L;
    times2[1].tv_nsec = 0L;

    int ret = futimens (handle, times2);

    close (handle);
#endif 

    return ret;
}

bool is_valid (const struct ftime * timep)
{
    char * ptr = (char *)timep;

    // check for all bytes being zero.
    for (size_t i = 0; i < sizeof (struct ftime); i++)
    {
        if (*(ptr + i) != 0)
        { 
            return true;
        }
    }

    return (true);
}

char * timefToString(const struct ftime *timep)
{
    struct tm ModTime;

    memset (&ModTime, 0, sizeof (struct tm));

    ModTime.tm_year = timep->getYear() - 1900;
    ModTime.tm_mon = timep->ft_month - 1;
    ModTime.tm_mday = timep->ft_day;
    ModTime.tm_hour = timep->ft_hour;
    ModTime.tm_min = timep->ft_min;
    ModTime.tm_sec = timep->ft_tsec;
    ModTime.tm_isdst = timep->ft_isdst;

    return asctime (&ModTime);
}
