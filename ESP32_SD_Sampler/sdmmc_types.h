#pragma once

#include "driver/sdmmc_host.h"
#include <vector>
#include <FixedString.h>

#define MAX_NAME_LEN (64)   // must be dividible by 4
#define MAX_PATH_LEN (128)  // must be dividible by 4
#define MAX_STR_LEN (256)   // max string size
#define BYTES_PER_SECTOR (512)
#define FAT_CACHE_SECTORS (8)
#define DIR_CACHE_SECTORS (8)
#define DIR_ENTRY_SIZE (32)
#define NDIR_PER_SEC (BYTES_PER_SECTOR/DIR_ENTRY_SIZE) 

using  fname_t = FixedString<MAX_NAME_LEN>   ;
using  fpath_t = FixedString<MAX_PATH_LEN>   ;
using  str_max_t = FixedString<MAX_STR_LEN>   ;

// directory attribute types.
enum {
  FAT32_NO_ATTR         = 0,
  FAT32_RO              = 0x01,
  FAT32_HIDDEN          = 0x02,
  FAT32_SYSTEM_FILE     = 0x04,
  FAT32_VOLUME_LABEL    = 0x08,
  FAT32_LONG_FILE_NAME  = 0x0F,
  FAT32_DIR             = 0x10,
  FAT32_ARCHIVE         = 0x20,
  FAT32_DEVICE          = 0x40,
  FAT32_RESERVED        = 0x80,
} eAttr_t;

enum {
  FREE_CLUSTER          = 0,
  RESERVED_CLUSTER      = 0x01,
  USED_CLUSTER          = 0x02,
  BAD_CLUSTER           = 0x0FFFFFF7,
  LAST_CLUSTER          = 0x0FFFFFF8,
};

typedef struct __attribute__((packed)) {       
  uint8_t   status; 
  uint8_t   headStart; 
  uint16_t  cylSectStart; 
  uint8_t   fsType;
  uint8_t   headEnd; 
  uint16_t  cylSectEnd; 
  uint32_t  firstSector;
  uint32_t  sectorsTotal; 
} part_t;

typedef struct __attribute__ ((packed)) {
  // if filename[0] == 0, then not allocated.
  // 2-11 char of file name.  the "." is implied b/n bytes 7 and 8
  char filename[11];        // 0-10 File name (8 bytes) with extension (3 bytes)
  uint8_t   attr;           // 11 Attribute - a bitvector. 
                            //    Bit 0: read only. 
                            //    Bit 1: hidden.
                            //    Bit 2: system file. 
                            //    Bit 3: volume label. 
                            //    Bit 4: subdirectory.
                            //    Bit 5: archive. 
                            //    Bits 6-7: unused.
  uint8_t   reserved0;      // 12     Reserved
  uint8_t   ctime_tenths;   // 13     file creation time (tenths of sec)
  uint16_t  ctime;          // 14-15  create time hours,min,sec
  uint16_t  create_date;    // 16-17  create date
  uint16_t  access_date;    // 18-19  create date
  uint16_t  hi_start;       // 20-21  high two bytes of first cluster
  uint16_t  mod_time;       // 22-23  Time (5/6/5 bits, for hour/minutes/doubleseconds)
  uint16_t  mod_date;       // 24-25  Date (7/4/5 bits, for year-since-1980/month/day)
  uint16_t  lo_start;       // 26-27  low order 2 bytes of first cluster
  // 0 for directories
  uint32_t file_nbytes;     // 28-31   Filesize in bytes
} sfn_dir_t;

typedef struct __attribute__ ((packed)) {
  uint8_t   seqno;          // 0-0 Sequence number (ORed with 0x40) and allocation 
                            // status (0xe5 if unallocated)   
  uint8_t   name1_5[10];    // 1-10    File name characters 1-5 (Unicode) 
  uint8_t   attr;           // 11-11   File attributes (always 0x0f)
  uint8_t   reserved;       // 12-12   Reserved   
  uint8_t   cksum;          // 13-13   Checksum  
  uint8_t   name6_11[12];   // 14-25   File name characters 6-11 (Unicode) 
  uint16_t  reserved1;      // 26-27   Reserved   
  uint8_t   name12_13[4];   // 28-31   File name characters 12-13 (Unicode) 
} lfn_dir_t;

typedef struct {
  uint32_t first;
  uint32_t last;
} chain_t;

typedef struct {
  fname_t   name;
  uint32_t  size;
  int       is_dir;
  int       is_end;
  std::vector<chain_t> sectors;
} entry_t;

typedef struct {
  entry_t entry;
  fpath_t currentDir;
  uint32_t curSector  ;
  uint32_t curCluster ;
  uint32_t startSector;
  uint32_t startCluster;
  uint32_t firstCachedFatSector;
  uint32_t firstCachedDirSector;
  int      direntNum ; 
} point_t;

typedef std::vector<entry_t> dirList_t;
