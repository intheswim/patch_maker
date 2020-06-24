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

#include <new>
#include <assert.h>

#define USE_MAP

#ifdef USE_MAP
#include <unordered_map>
#include <vector>
#include <utility>
#endif

#include <algorithm> // lower_bound

#include "common.h"
#include "progArgs.h"

#include "checksum.h"
#include "timeutil.h"

#include <chrono>

#define W_BUFFER_SIZE 4096L
#define USE_STD_BINARY_SEARCH  // only used wjen USE_MAP is off

#define USE_MAP // comment out to use sorted list of RefStructs and binary search. The difference in performance is marginal.

#ifndef USE_MAP

struct RefStruct 
{
    uint32_t   checkValue;
    uint32_t   index;

    static int compareRefs (const void * _a, const void * _b)
    {
        const RefStruct * a = (RefStruct *)_a;
        const RefStruct * b = (RefStruct *)_b;

        if (a->checkValue < b->checkValue) return -1;
        if (a->checkValue > b->checkValue) return 1;

        if (a->index < b->index) return -1;
        
        if (a->index > b->index) return 1;

        return 0;
    }

#ifdef USE_STD_BINARY_SEARCH
    static bool ref_comp (const RefStruct & a, const unsigned long val)
    {
        return (a.checkValue < val);
    }
#endif     

    static long RefIndex (unsigned long cSum, const RefStruct *elements, const int iCount)
    {
#ifndef USE_STD_BINARY_SEARCH        
        long index = 0, left, right;
        left = 0;
        right = iCount - 1;

        while (left <= right)
        {
            index = (left + right) >> 1;
            if (cSum == elements[index].checkValue) break;
            else if (cSum < elements[index].checkValue) right = index - 1;
            else left = index + 1;
        }

        if (cSum != elements[index].checkValue) return (-1L);

        while (index > 0 && elements[index - 1].checkValue == cSum) index--;

        return index;
#else         

        RefStruct *rs = std::lower_bound (elements, elements + iCount, cSum, ref_comp);

        if (rs == nullptr) return -1L;

        if (rs->checkValue != cSum) return -1;

        return rs - elements;
#endif         
    }

};
#endif // USE_MAP

// input is 8 byte buffer
static uint32_t checkSum (const unsigned char *szBuff32, bool & left_right_match)
{
    uint32_t lSum, iLSum = 0, iRSum = 0, lChar;

    int j = 0;

    for (; j < 4; j++) { iRSum += szBuff32[j]; }

    lSum = iRSum;
    
    for (; j < 8; j++) { iLSum += szBuff32[j]; }

    left_right_match = (iLSum == iRSum);

    lSum += (iLSum << 10);
    
    lSum += ((long)szBuff32[7] & 0x0F) << 20;

    lChar = *szBuff32;
    
    lSum += (lChar << 24);

    return lSum;
} 

// input is 8 byte buffer
static bool EqSequence (const unsigned char *szBuff32) // all 8 bytes are the same
{
    int i, j;
    
    j = *szBuff32;

    for (i = 1; i < 8; i++) 
    {
        if (szBuff32[i] != j) break;
    }

    return (i == 8);
}

#ifdef USE_MAP

static bool WorthReferring (const std::vector<uint32_t> * indexes, uint32_t & maxL, uint32_t & iStart, const long pos,
            const long blockSize, FILE *fp1, FILE *fp2, const size_t file1_size, const size_t file2_size, bool & rwError)
{
    long j, lStart, lLongest = 0;
    uint32_t iLongest = 0;

    char buffer1 [W_BUFFER_SIZE], buffer2[W_BUFFER_SIZE];

    if (indexes == nullptr || indexes->empty()) return false;

    for (auto i : *indexes)
    {
        lStart = (long)i * blockSize;

        // indexes are in ascending order, so we can break if:
        if (lStart + (size_t)lLongest > file1_size) { break; }
        if (pos + (size_t)lLongest > file2_size) { break; }

        if (0 != fseek(fp1, lStart, SEEK_SET)) rwError = true;

        if (0 != fseek(fp2, pos, SEEK_SET)) rwError = true;

        if (rwError)
        {
            return false;
        }

        j = 0;

        if (lLongest > 0)
        {
            long len = lLongest;

            int mismatch = 0;

            while (len > 0)
            {
                size_t mx = (std::min)(len, W_BUFFER_SIZE);

                if (fread (buffer1, mx, 1, fp1) != 1) { mismatch = 1; break; }
                if (fread (buffer2, mx, 1, fp2) != 1) { mismatch = 1; break; }

                if (memcmp (buffer1, buffer2, mx) != 0)
                {
                    mismatch = 1;
                    break;
                }
                len -= (long)mx;
            }

            if (mismatch) continue;

            j = lLongest;
        }  

        for (int mis = 0; mis == 0; )
        {
            size_t r1 = fread (buffer1, 1, 16, fp1);
            size_t r2 = fread (buffer2, 1, 16, fp2);

            size_t mx = (std::min) (r1, r2);

            for (size_t z = 0; z < mx; z++)
            {
                if (buffer1[z] != buffer2[z]) { mis = 1; break; }
                j++;
            }

            if (mx < 16) break;
        }  

        if (j > lLongest)
        {
            lLongest = j;
            iLongest = i;
        }
    }

    fseek(fp2, pos, SEEK_SET);
  
    if (lLongest >= 8)
    {
        maxL = lLongest;
        iStart = iLongest;

        return true;
    }
  
    return false;
}
#else 
static bool WorthReferring (const long index, uint32_t & maxL, uint32_t & iStart, const long pos,
            const long blockSize, const RefStruct *refs, const long iCount, FILE *fp1, FILE *fp2, bool & rwError)
{
    long i, j, lStart, lLongest = 0;
    uint32_t iLongest = 0;
    int byte1, byte2;

    char buffer1 [W_BUFFER_SIZE], buffer2[W_BUFFER_SIZE];

    if (index < 0) return false;

    for (i = index; i < iCount; i++)
    {
        if (refs[i].checkValue != refs[index].checkValue) break;

        lStart = (long)refs[i].index * blockSize;

        if (0 != fseek(fp1, lStart, SEEK_SET)) rwError = true;

        if (0 != fseek(fp2, pos, SEEK_SET)) rwError = true;

        if (rwError)
        {
            return false;
        }

        j = 0;

        if (lLongest > 0)
        {
            long len = lLongest;

            int mismatch = 0;

            while (len > 0)
            {
                size_t mx = (std::min)(len, W_BUFFER_SIZE);

                if (fread (buffer1, mx, 1, fp1) != 1) { mismatch = 1; break; }
                if (fread (buffer2, mx, 1, fp2) != 1) { mismatch = 1; break; }

                if (memcmp (buffer1, buffer2, mx) != 0)
                {
                    mismatch = 1;
                    break;
                }
                len -= (long)mx;
            }

            if (mismatch) continue;

            j = lLongest;
        }

        for (; ; j++)
        {
            byte1 = fgetc(fp1);
            byte2 = fgetc(fp2);

            if (byte1==EOF || byte2==EOF) break;
            if (byte1 != byte2) break;
        }

        if (j > lLongest)
        {
            lLongest = j;
            iLongest = refs[i].index;
        }
    }

    fseek(fp2, pos, SEEK_SET);
  
    if (lLongest >= 8)
    {
        maxL = lLongest;
        iStart = iLongest;

        return true;
    }
  
    return false;
}
#endif 

////////////////////////////////////////////////////////////////////////////////////////////

static int createPatchBody (AutoClose & ac, 
#ifndef USE_MAP
            RefStruct *refs, int refCount, 
#else  
            const std::unordered_map<uint32_t, std::vector<uint32_t> > & u_map,           
#endif 
            long blockSize, long size1, long size2, const progArguments & args)
{
    unsigned char szBuff32 [8];

    unsigned char szBuff [256];

    bool chStarted = false ;  // changed sequence. It was not found in file1 so it must be written as is.

    bool rwError = false;

    uint8_t bLen = 0;

    int32_t iLen = 0;
#ifndef USE_MAP    
    long index = 0;
#else 
    const std::vector<uint32_t> * indexes_ptr = NULL;
#endif     

    PatchStats stats;

    FILE *fp1 = ac.fp1;
    FILE *fp2 = ac.fp2;
    FILE *fp3 = ac.fp3;

    long pos = 0;

    for (pos = 0; (pos < size2) && !rwError; )
    {
        if (size2 - pos >= 8)
        {
            if (chStarted)
            {
                fseek(fp2, pos + 7, SEEK_SET);

                for (int i=0; i < 7; i++) { szBuff32[i] = szBuff32[i+1]; }
                
                if (fread(szBuff32 + 7, 1, 1, fp2) != 1) { rwError = true; }
            }
            else
            {
                fseek(fp2, pos, SEEK_SET);
                if (fread(szBuff32, 8, 1, fp2) != 1) { rwError = true; }
            }

            bool leftRightMatch = false;

            unsigned long lSum = checkSum (szBuff32, leftRightMatch);

            if (lSum == 0L || (leftRightMatch && EqSequence (szBuff32)))
            {
                if (chStarted)
                {
                    stats.as_is_length += iLen;
                    stats.as_is_count++;

                    bLen = iLen - 1;
                    fwrite (&bLen, 1, 1, fp3);
                    if (fwrite(szBuff, iLen, 1, fp3) != 1) { rwError = true; }
                    chStarted = 0;
                }

                iLen = 8;
                pos += 8;
        
                while ((iLen < 256) && (getc(fp2) == *szBuff32))
                    pos++, iLen++;

                bLen = BYTE_PATCH_SEQ;  // 4 is flag for sequence.
        
                fwrite (&bLen, 1, 1, fp3);
                bLen = iLen - 1;
                fwrite (&bLen, 1, 1, fp3);
                bLen = *szBuff32;
                if (fwrite (&bLen, 1, 1, fp3) != 1) { rwError = true; }
                continue;
            }
            else 
            {
#ifndef USE_MAP                
                index = RefStruct::RefIndex (lSum, refs, refCount);
#else 
                auto it = u_map.find(lSum);

                if (it == u_map.end())
                {
                    indexes_ptr = NULL;
                }
                else 
                {
                    indexes_ptr = &(it->second);
                }
#endif                 
            }
        }
        else // last 8 bytes of "new" file, we cannof fully read all 8 bytes.
        {
            fseek (fp2, pos, SEEK_SET);
            if (fread (szBuff32, 1, 1, fp2) != 1) { rwError = true; }
#ifndef USE_MAP            
            index = -1L;
#else 
            indexes_ptr = NULL;
#endif             
        }

        uint32_t maxLen = 0;

        uint32_t iStart = 0;

#ifndef USE_MAP
        bool useRef = WorthReferring (index, maxLen, iStart, pos, blockSize, refs, refCount, fp1, fp2, rwError);
#else 
        bool useRef = WorthReferring (indexes_ptr, maxLen, iStart, pos, blockSize, fp1, fp2, size1, size2, rwError);
#endif         

        if (rwError) break;

        if (!useRef)
        {
            if (chStarted)
            {
                pos++;
                szBuff[iLen] = *szBuff32;
                iLen++;
                if (iLen == 256)
                {
                    stats.as_is_length += 256;
                    stats.as_is_count++;
                    stats.as_is_maxed++;

                    bLen = 255;
                    fwrite(&bLen, 1, 1, fp3);
                    if (fwrite(szBuff, 256, 1, fp3) != 1) rwError = true;
                    chStarted = 0;
                }
            }
            else
            {
                pos++;
                bLen = 0;
                fwrite (&bLen, 1, 1, fp3);
                *szBuff = *szBuff32;
                chStarted = 1;
                iLen = 1;
            }
        }
        else // referring pos.
        {
            stats.refs_length += maxLen;
            stats.refs_count++;
            stats.refs_longest = (std::max)(stats.refs_longest, (long)maxLen);

            pos += maxLen;

            if (chStarted)
            {
                stats.as_is_length += iLen;
                stats.as_is_count++;

                bLen = iLen-1;
                fwrite (&bLen, 1, 1, fp3);
                if (fwrite(szBuff, iLen, 1, fp3) != 1) { rwError = true; }
                chStarted = 0;
            }

            uint16_t wStart = iStart & 0xFFFF;

            iStart >>= 16;

            if (maxLen <= 255)
            {
                bLen = (iStart << 2) + 1;

                fwrite (&bLen, 1, 1, fp3);
                fwrite (&wStart, 2, 1, fp3);
                bLen = maxLen;
                if (fwrite(&bLen, 1, 1, fp3) != 1) { rwError = true; }
            }
            else if (maxLen <= 0xFFFFL)
            {
                bLen = (iStart << 2) + 2;

                fwrite (&bLen, 1, 1, fp3);
                fwrite (&wStart, 2, 1, fp3);
                iLen = maxLen;
                if (fwrite(&iLen, 2, 1, fp3) != 1) { rwError = true; }
            }
            else
            {
                bLen = (iStart << 2) + 3;

                fwrite (&bLen, 1, 1, fp3);
                fwrite (&wStart, 2, 1, fp3);
                if (fwrite(&maxLen, 4, 1, fp3) != 1) { rwError = true; }
            }
        }
    }  // end for

    // write the tail portion

    if (chStarted && !rwError)
    {
        stats.as_is_length += iLen;
        stats.as_is_count++;

        bLen = iLen-1;
        fwrite(&bLen, 1, 1, fp3);
        if (fwrite(szBuff, iLen, 1, fp3) != 1) { rwError = true; }
    }

    if (!rwError)
    {
        bLen = BYTE_PATCH_EOF;
        if (fwrite(&bLen, 1, 1, fp3) != 1) { rwError = true; }

        if (args.flagShowStats)
        {
            printf ("\tAs-is length : %ld\n", stats.as_is_length);
            printf ("\tAs-is count  : %ld (maxed %ld)\n", stats.as_is_count, stats.as_is_maxed);
            printf ("\tRefs count   : %ld\n", stats.refs_count);
            printf ("\tRefs length  : %ld\n", stats.refs_length);
            printf ("\tRefs longest : %ld\n", stats.refs_longest);
        }
    }

    return (rwError == false) ? 0 : 1;  
}

///////////////////////////////////////////////////////////////////////////
//
//  This is the only exposed function from this file.
//
///////////////////////////////////////////////////////////////////////////

int createPatch (const struct progArguments & args)
{
    // get timestamps
    struct ftime time1 = ftime(), time2 = ftime();
    long fsize1 = 0, fsize2 = 0;

    if (sizeof(struct ftime) != 5)
    {
      fprintf(stderr, "Wrong time struct size. Cannot proceed.\n");
      return -1;
    }

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
  
    int ret = getftime (args.file1, &time1, &fsize1);

    if (0 != ret)
    {
        fprintf(stderr, "Could not get latest modification time for %s\n", args.file1);
        return EXIT_FAILURE;
    }

    if (fsize1 == 0)
    {
        fprintf(stderr, "Input file %s size zero\n", args.file1);
        return EXIT_FAILURE;
    }
      
    ret = getftime (args.file2, &time2, &fsize2);

    if (0 != ret)
    {
        fprintf(stderr, "Could not get latest modification time for %s\n", args.file2);
        return EXIT_FAILURE;
    }

    if (args.flagVerbose)
    {
        // print latest modif. times.
        printf ("Latest modification time 1 : %s", timefToString (&time1) );
        printf ("Latest modification time 2 : %s", timefToString (&time2) );
    }

    struct AutoClose ac;

    ac.fp1 = fopen(args.file1, "rb");

    if (ac.fp1 == NULL)
    {
        fprintf (stderr, "Could not open file %s\n", args.file1);
        return EXIT_FAILURE;
    }

    ac.fp2 = fopen (args.file2, "rb");

    if (ac.fp2 == NULL)
    {
        printf("Could not open file %s\n", args.file2);
        return EXIT_FAILURE;
    }

    // try buffering entire file in memory if possible.
    if (false == args.flagDiskIO)
    { 
        int  memb_cnt = 0;
        ac.buffer1 = (char *)malloc (fsize1);
        if (ac.buffer1)
        {
            memb_cnt++;
            setvbuf (ac.fp1, ac.buffer1, _IOFBF, fsize1);
        }

        ac.buffer2 = (char *)malloc (fsize2);
        if (ac.buffer2)
        {
            memb_cnt++;
            setvbuf (ac.fp2, ac.buffer2, _IOFBF, fsize2);
        }

        if (memb_cnt == 2 && args.flagVerbose)
        {
            printf("Full disk I/O buffering enabled\n");
        }
    }

    long size1 = 0, size2 = 0;

    fseek (ac.fp1, 0L, SEEK_END);
    size1 = ftell(ac.fp1);
    fseek (ac.fp2, 0L, SEEK_END);
    size2 = ftell(ac.fp2);

    if ((size1 != fsize1) || (size2 != fsize2))
    {
        fprintf(stderr, "Stat and actual file size mismatch. Exiting.\n");
        return EXIT_FAILURE;
    }

    // get checksum of file 1.

    uint32_t checksum1 = 0, checksum2 = 0;

    if (0 != getChecksum (ac.fp1, checksum1))
    {
        fprintf (stderr, "Failed calculating checksum\n");
        return EXIT_FAILURE;
    }

    // get checksum of file 2.

    if (0 != getChecksum (ac.fp2, checksum2))
    {
        fprintf (stderr, "Failed calculating checksum\n");
        return EXIT_FAILURE;
    }

    if (args.flagVerbose)
    {
        printf ("Cksum 1: %X\n", (int)checksum1);
        printf ("Cksum 2: %X\n", (int)checksum2);
    }

    if ((checksum2 == checksum1) && (size1 == size2))
    {
        if (!args.flagForce)
        {
            printf ("Two input files seem to be identical. Nothing to do.\n");
            return EXIT_SUCCESS;
        }
        else 
        {
            printf ("Two input files seem to be identical. Forcing output.\n");
        }
    }

    if (access(args.file3, F_OK) == 0 && !args.flagForce)
    {
        printf("File %s already exists. Use -f flag to overwrite.\n", args.file3);
        return EXIT_SUCCESS;
    }

    ac.fp3 = fopen(args.file3, "w+b");

    if (ac.fp3 == NULL)
    {
        fprintf (stderr, "Could not open patch file.\n");
        return EXIT_FAILURE;
    }

    long blockSize = 0;

    if ((size1 - 8) < 0x40000L) 
        blockSize = 4;
    else
    {
        blockSize = (size1 - 8) >> 16;
        if ((blockSize << 16) < size1 - 8) blockSize++;

        if (args.flagMaximizeCompression)
        {
            blockSize = (size1 - 8) >> 18;
            if ((blockSize << 18) < size1 - 8) blockSize++;
        }
    }

    if (blockSize > 256)
    {
        printf ("Block size too large (%ld)\n", blockSize);
        return EXIT_FAILURE;
    }

    if (args.flagVerbose)
    {
        printf ("Block size %ld\n", blockSize);
    }

    unsigned long iCount = (size1 - 8 + blockSize) / blockSize;

    if (iCount > 0x3FFFFF)
    {
        fprintf (stderr, "Input file too large.\n");
        return EXIT_FAILURE;
    }

    if (args.flagVerbose)
    {
        printf ("Allocating %ld ref. points\n", iCount);
    }

#ifndef USE_MAP
    RefStruct *rv = new (std::nothrow) RefStruct [iCount];

    if (!rv) 
    {             
        fprintf (stderr, "Not enough memory\n");
        return EXIT_FAILURE;
    }  

#else 
    std::unordered_map<uint32_t, std::vector<uint32_t> > u_map; 
    u_map.reserve (iCount);

#endif     

    long pos = 0;
    unsigned long i = 0;
    unsigned char szBuff32 [8];

    for (i = 0, pos = 0; i < iCount; i++, pos += blockSize)
    {
        if (0 != fseek (ac.fp1, pos, SEEK_SET)) break;

        if (fread(szBuff32, 8, 1, ac.fp1) != 1) break;

        bool dummy = false;

#ifndef USE_MAP
        rv[i].checkValue = checkSum (szBuff32, dummy); 
        rv[i].index = i;
#else 
        uint32_t sum = checkSum (szBuff32, dummy);

        u_map[sum].push_back (i);
#endif         
    }

    if (i < iCount)
    {
        fprintf(stderr, "Read error. Exiting.\n");
        return EXIT_FAILURE;
    }

#ifndef USE_MAP
    qsort (rv, iCount, sizeof(struct RefStruct), RefStruct::compareRefs);
#endif     

    // write header output ///////////////////////////////////////////////////

    unsigned char szHead [6] = "FOC";

    szHead[3] = _VERSION_;
    szHead[4] = (unsigned char)(blockSize - 1);
    szHead[5] = 0; // set to 0 to indicate that we have not finished (aborted). Set to endianness (1 or 2) when completed. 
    fwrite (szHead, 6, 1, ac.fp3);

    fwrite (&time1, 5, 1, ac.fp3);
    fwrite (&time2, 5, 1, ac.fp3);

    // we are expecting file size to fit 4 bytes.
    uint32_t _size1 = size1;
    uint32_t _size2 = size2;

    assert (_size1 == size1);
    assert (_size2 == size2);

    if (args.flagVerbose)
    {
        printf("Input file 1 size: %ld\n", (long)_size1);
        printf("Input file 2 size: %ld\n", (long)_size2);
    }

    fwrite (&_size1, 4, 1, ac.fp3);
    fwrite (&_size2, 4, 1, ac.fp3);

    fwrite (&checksum1, 4, 1, ac.fp3);
    fwrite (&checksum2, 4, 1, ac.fp3);

    // End of header. Now write patch body.

#ifndef USE_MAP
    ret = createPatchBody (ac, rv, iCount, blockSize, size1, size2, args);
    delete[] rv;
#else 
    ret = createPatchBody (ac, u_map, blockSize, size1, size2, args);    
#endif     

    if (ret != 0)
    {
        fclose (ac.fp3);
        ac.fp3 = nullptr;

        fprintf(stderr, "Read/write error\n");
        remove(args.file3);
    }

    if (ret == 0) // set byte 6 to zero to indicate completion. 
    {
        fseek (ac.fp3, 5, SEEK_SET);
        unsigned char byte = getEndianness();
        fwrite (&byte, 1, 1, ac.fp3);

        if (!args.flagQuiet)
        {
            char buffer [256];
            printf ("%s\n", getEndiannessString (buffer, 256));

            auto end = std::chrono::high_resolution_clock::now();
        	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);        

			printf ("Patch completed in %.2lf seconds\n", 0.001 * duration.count());
        }

        if (args.flagVerbose)
        {
            fseek(ac.fp3, 0L, SEEK_END);
            long size = ftell(ac.fp3);

            printf ("Patch size %ld bytes (or %.2lf%%)\n", size, 100.0 * (double)size / size2);
        }
    }

    return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


