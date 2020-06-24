/* Patch making and patch applying utility, based on my original work from 1997.

Description: 

Patch maker: given two similar  (in terms of contents)  files (file #1 and file 
#2) as input  create a patch (file #3)  which  has  description of  differences 
between the two input files.

Patch applying function takes file #1 and patch file as input, and outputs file #2.

When difference between file #1 and file #2 is small compared to their size, we 
can achieve significant savings when sending / storing patches. 


    PATCH FILE SCHEMA:

    HEADER (32 bytes, numbered from 0):

    BYTES 0-2       have "FOC" in them.
    BYTE    3       has version number. This is to be used by "patch applying part" 
                    (it must verify that this number matches the software version).
    BYTE    4       contains block size minus 1. Block size can be anywhere from 1 to 256.
                    (higher values are not practical).
    BYTE    5       set to zero when patch is in progress. 
                    when patch is completed indicates endianness (1 - little, 2 - big).
                    Endianness (along with version) should be checked when applying a patch.
    BYTES 6-10      has file 1 last modification timestamp. Can be zero.
    BYTES 11-15     has file 2 last modification timestamp. Can be zero.
    BYTES 16-19     has file 1 size in bytes.
    BYTES 20-23     has file 2 size in bytes
    BYTES 24-26     has checksum for file1.
    BYTES 28-31     has checksum for file2.

    END OF HEADER

    BODY. Has a number of sequences:

    Sequence start with byte (V).

    When last 2 bits of V have value 1-3, it means they contain information 
    (let's call it LEN).
    
        In this  case read  next 2 bytes to get part of  the block index from 
        file #1 to copy. Combine with higher 6 bits of V to get this index (it 
        can be 22 bits long, so total number of blocks can be 4.2 million).
        Then LEN value has the number of bytes to read to get the <length> of 
        the block to copy.

        1 - read one byte,
        2 - read two bytes.
        3 - read four bytes.

        Copy <length> bytes starting from position (block_index * block_size) 
        in file #1 to output.

    When last 2 bits of V are set to zero, read next 2 lowest bits of V, i.e. 
    (( V >> 2) & 3):

        00 - means there is a sequence of bytes to write which follows.
             Read next byte to get the length of the sequence (less one).

        01 - means there's a sequence of  identical bytes to write.  Read next 
             byte to get the sequence length (less one), and the byte after to 
             get the value two write.

        10 - special value for EOF. Terminate output and compare output length 
             to header info.

        11 - not used.


Arguments for program:

    -c  create patch.
    -a  apply patch.
        (one of these 2 are required)
    -q  quiet mode  - minimum output
    -v  verbose mode - maximum aoutput
    -f  force overwriting patch or file2 if present
    -s  print stats (creation only)
    -i  ignore timestamp information
    -d  disable I/O buffering
    -x  maximize compression";
    
    syntax: ./patch [caqfsei] file1 file2 [patch]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "progArgs.h"
  
int main (int argc, char *argv[])
{
    struct progArguments args;

    if (0 != args.parseArguments (argc, argv))
    {
        return EXIT_SUCCESS; // invalid arguments.
    }

    // at this point file name are set and flags are valid.

    if (args.flagCreate)
    {
        return createPatch (args);
    }
    else 
    {
      return applyPatch (args);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

