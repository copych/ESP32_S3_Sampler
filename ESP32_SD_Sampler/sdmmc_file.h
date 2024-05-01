#pragma once

// helper class, it is not that fast and clear, but sometimes you just want to read lines from a config file 
// (here it's limited to 256-1 bytes by FixedString, but you can refactor it and use String MAYBE),
// though consider using the full SDMMC/FS combo or vfs (esp_vfs_fat_sdmmc_mount()) in that case
#include "sdmmc_types.h"

class SDMMC_FileReader {
  public:  
    SDMMC_FileReader(SDMMC_FAT32* Card);
    ~SDMMC_FileReader(){};
    esp_err_t   open(fname_t fname);
    esp_err_t   close();
    void        read_line(str_max_t& retStr); // we assume that line length < FixedString max size (256 bytes)
    bool        available();
    void        rewind();
    entry_t     next_entry();
    
  private:
    SDMMC_FAT32*    _Card;
    char            _sectorBuf[BYTES_PER_SECTOR];
    uint32_t        _sectorRead;  
    fpath_t         _path;
    entry_t         _entry;
    entry_t         _currentEntry;
    uint32_t        _filePos; // promote to size_t if you'd like
    uint32_t        _bufPos;
    uint32_t        _bufCount;
    uint32_t        _dirPos;
};