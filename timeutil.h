#pragma once

#pragma pack(push,1)
struct ftime
{
   unsigned ft_tsec : 6;
   unsigned ft_min : 6;
   unsigned ft_hour : 5;
   unsigned ft_day : 5;
   unsigned ft_month : 4;
   unsigned ft_year_part : 3; // years 0 - 8191
   unsigned ft_isdst : 1;
   unsigned char ft_year_ext;

   void setYear(unsigned int year)
   {
     ft_year_part = year & 7; // 3 bits
     ft_year_ext = year >> 3;
   }
   int getYear() const
   {
     return ((ft_year_ext << 3) | ft_year_part);
   }
} ;
#pragma pack(pop)

// get last file modification time and (optionally) size. returns zero on success.
int getftime (const char *filename, struct ftime *ftimep, long * fsize = NULL);

// set last file modification and access time. returns zero on success.
int setftime (const char *filename, const struct ftime * timep);

bool is_valid (const struct ftime * timep);

char * timefToString(const struct ftime *timep);