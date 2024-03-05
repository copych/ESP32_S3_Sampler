#pragma once

#define   BUF_NUMBER        2      // tic-tac don't change, for readability only
#define   CHANNELS          2      // 1 = mono, 2 = stereo
#define   BYTES_PER_CHANNEL 2
const int   BUF_SIZE_BYTES =   (READ_BUF_SECTORS * BYTES_PER_SECTOR);
const int   BUF_SIZE_INTS  =   (BUF_SIZE_BYTES / 2);
const int   INTS_PER_SECTOR =  (BYTES_PER_SECTOR / 2);

#include "adsr.h"
#include "sdmmc.h"

enum eMemType_t { MEM_RAM, MEM_PSRAM }; 

typedef struct __attribute__((packed)){
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;                    // actually, ( filesize-8 )
  char waveType[4] = {'W', 'A', 'V', 'E'};
  char format[4] = {'f', 'm', 't', ' '};
  uint32_t lengthOfData = 16;           // length of the rest of the header : 16 for PCM
  uint16_t audioFormat = 1;             // 1 for PCM
  uint16_t numberOfChannels = 2;        // 1 - mono, 2 - stereo
  uint32_t sampleRate = 44100;        
  uint32_t byteRate = 44100 * 2 * 2;    // sample rate * number of channels * bytes per sample
  uint16_t blockAlign = 16 / 8  * 2;    // bytes per sample (all channels)
  uint16_t bitsPerSample = 16;
  char dataStr[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;  
} wav_header_t;

typedef struct {
  int       byte_offset;
  size_t    size;
  float     orig_freq;
  float     speed     = 0.0f; // samples
  uint8_t   orig_velo_layer;
  int       sample_rate   = 44100;
  int       channels      = -1;
  int       bit_depth     = -1;
  float     amp           = 1.0f;
  float     attack_time   = 0.01f;
  float     decay_time    = 0.1f;
  float     release_time  = 5.0f;
  std::vector<chain_t>   sectors;
} sample_t;

class Voice {
  public:
    Voice(){};
    void  init(SDMMC_FAT32* Card);
    bool  allocateBuffers(eMemType_t mem_type);
    void  getSample(float &sampleL, float &sampleR);
    void  prepare(const sample_t& smp);
    void  start(uint8_t midiNote, uint8_t velocity);
    void  end(bool hard);
    void  feed();
    inline uint32_t   hunger();
    inline bool       isActive()    {return _active;}
    inline uint8_t    getMidiNote(){return _midiNote;}
    inline uint32_t   getBufPlayed() {return _bufPlayed;}
    inline void       toggleBuf();
    inline float      interpolate(float& s1, float& s2, float i);
    
  private:
    SDMMC_FAT32*    _Card;
    volatile bool      _active                 = false;
    int       _sampleId               = -1;
    volatile int16_t*  _buffer0;                 // 
    volatile int16_t*  _buffer1;                 // 
    volatile int16_t*  _playBuf;
    volatile int16_t*  _fillBuf;
    uint32_t  _bufSizeBytes           = READ_BUF_SECTORS * BYTES_PER_SECTOR;
    uint32_t  _bufSizeSmp             = 0;
    uint32_t  _hunger                 = 0;
    volatile int       _bufPosSmp[2]           = {0, 0}; // sample pos, i.e. it depends on the number of channels and bit depth of a wav file assigned to this voice;
    float     _bufPosSmpF             = 0.0;    // exact calculated sample reading position including speed, pitchbend etc. 
    volatile bool      _bufEmpty[2]             = {true, true};
    uint32_t  _smpSize                = 4;      // bytes
    uint32_t  _filePosSmp             = 0;
    uint32_t  _filePosBytes           = 0;
    uint32_t  _bytesRead              = 0;
    volatile int       _idToFill              = 0;      // tic-tac buffer id
    volatile int       _idToPlay              = 1;
    int       _outputChannels         = 2;      // 2 is stereo, 1 is mono
    bool      _alwaysMono             = false;  // if stereo sample given, take only L channel. _outputChannels param still makes effect
    int       _sampleBytes            = 4;      // 2 is mono 16 bit, 4 is stereo 16 bit
    uint8_t   _midiNote               = 0;
    float     _speed                  = 1.0;    // _speed param corrects the central freq of a sample 
    float     _speedModifier          = 1.0;    // pitchbend, portamento etc. 
    uint8_t   _midiVelo               = 0;
    uint8_t   _veloLayer              = 0;
    uint32_t  _lastSectorRead         = 0;
    uint32_t  _curChain               = 0;
    uint32_t  _bufPlayed              = 0;
    float     _lackingL               = 0.0;
    float     _lackingR               = 0.0;
    volatile bool      _eof                    = true;
    sample_t  _sampleFile             ;
    Adsr      AmpEnv                  ;
};
