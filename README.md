#### Patch : patch making and applying utility.

<pre>
Originally developed as MS-DOS command line utility back in 1997, when computer 
memory was  quite limited  and  early Internet  speeds  very  slow, in order to 
transfer binary patches as opposed to full executables.

The utility was originally split into two separate  executables (named `script` 
and `play`). The current version  combinies both functions in one (quite small) 
executable called `patch`, which performs both creation of patches and patching 
(see flags below). 

As it turned  out,  over the years  many similar utilities  have been developed 
(both commerical and free).

I am publishing this code mostly for nostalgic reasons. I made some performance 
fixes, switched parts of code to modern C++ and made sure  it compiles and runs 
on  both  modern Linux (tested  on Ubuntu 18.04) and Windows 10 (it's slower on 
Windows - because of the slower disk I/O, see note on performance below).

</pre>

#### Usage

</pre>
type `./patch` to see all options.

`Syntax: ./patch [flags] oldfile.ext newfile.ext [file.pat]`
     `   flags: -c - create patch`
     `          -a - apply patch`
     `          -f - force overwrite`
     `          -i - ignore timestamps`
     `          -s - print stats`
     `          -q - quiet mode`
     `          -v - verbose mode`
     `          -d - disable I/O buffering`
     `          -x - maximize compression`

Flags can be combined into a single argument. Example: 

`./patch -cfv picture1.bmp picture2.bmp patch.foc` 

The utility can be used on  both binary and text files.  It treats all files as 
binary.

</pre>

#### Creating a patch file

<pre>
The `patch -c` option  creates a patch file with  specified  file name or (when
not set)  automatically  assigns the  patch  file name. The  output file can be
compressed  further  with  compression  software (such  as  zip, 7zip etc). 
</pre>

#### Appying a patch

<pre>
The `patch -a`  option applies  existing  patch  to  the `oldfile`, and  should
produce `newfile` (identical  to  that  given when  creating  a patch). It will
check timestamp, length  and CRC checksum of `oldfile` (prior to execution) and
of the `newfile` (after creating it) to verify that all went correctly. It will 
also apply the latest  modification timestamp  to `newfile`  unless `-i` option
is given.
</pre>

#### Algorithm description and patch schema

<pre>
Algorithm creates a hashtable (or a sorted array - with  optional #define) with
checksums and  corresponding  file positions evenly distributed over `oldfile`.
It then searches for the longest match between current position inside `newfile`
to find the duplicate parts. If such part is at least eight bytes long, it gets
referenced inside  the  patch. Otherwise it  writes  sequences  of  bytes  from
`newfile` to   the  patch, until  such matching  part is  located. An important
difference  of  this utility  from similar packages is that it also  checks for
long sequences of identical bytes and compresses them as well.    

The patch file structure/schema is described in detail inside `main.cpp`.
</pre>

#### Performance on Windows 10.

<pre>
A quick test of the  patch-making prt compiled with Visual Studio on Windows 10
shows that it runs substantially slower compared to Linux version, on a similar 
hardware - release and all optimization flags being set. Visual Studio profiler
shows the bottleneck is I/O functions such as `fread()`.

As result, I added setvbuf() calls to use  memory to speed up disk I/O. You can
disable I/O buffering with `-d` option, to save more memory for hash table.
</pre>

#### Limitations of current version

<pre>
The input file size of `oldfile` is  limited to 1GB. This number is  product of
22-bit  unsigned integer  used as block id, and 256, the current maximum  block
size. The  simplest way to increase  this limit  is to make  maximum block size
larger (1024 or more) - the header's byte #4  and  unused 6 bits of byte #5 can
be used to store new block size.
</pre>

#### Possible improvements

<pre>
The current maximum memory used for block ids hashtable table (asuming all keys
being unique) is 2^22 times 8 bytes, or 2^25 (corresponds to 32 MB). As of 2020, 
most modern computers have  gigabytes of RAM, which  means we have an option of 
increasing  the  maximum hashtable size to 2^30 - block ids will still fit into 
32-bit number in memory, but we will need to use one extra byte for block id in 
output  patch file - only  when  input file  size  is large enough - making the 
maximum memory requirement to be 8GB.

The  header also  stores file  sizes as unsigned 32-bit integers, limiting file 
size to 4GB. This can easily be fixed by changing to 48-bit or 64-bit integers.
</pre>

#### License

[MIT](https://choosealicense.com/licenses/mit/)