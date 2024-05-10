#pragma once
#define   BUF_EXTRA_BYTES   16       // for buffer overlapping
#define   BUF_NUMBER        2       // tic-tac don't change, for readability only
#define   CHANNELS          2       // 1 = mono, 2 = stereo
#define   BYTES_PER_CHANNEL 2
#define   FADE_OUT_SAMPLES  8      // fade-out pre-render samples on polyphony overrun
const int   BUF_SIZE_BYTES  =   (READ_BUF_SECTORS * BYTES_PER_SECTOR);
const int   BUF_SIZE_INTS   =   (BUF_SIZE_BYTES / 2);
const int   INTS_PER_SECTOR =   (BYTES_PER_SECTOR / 2);
const float MIDI_NORM       =   (1.0f / 127.0f); 

#include "adsr.h"
#include "sdmmc.h"

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
  int       byte_offset = 44;     // = wav header size
  uint32_t  size;
  float     orig_freq;
  float     speed         = 0.0f; // samples
  uint8_t   orig_velo_layer;
  int       sample_rate   = 44100;
  int       channels      = -1;
  int       bit_depth     = -1;
  float     amp           = 1.0f;
  float     attack_time   = 0.0f;
  float     decay_time    = 0.5f;
  float     sustain_level = 1.0f;
  float     release_time  = 12.0f;
  std::vector<chain_t>   sectors;
} sample_t;

class Voice {
  public:
    Voice(){};
    void  init(SDMMC_FAT32* Card, bool* sustain);
    bool  allocateBuffers();
    void getSample(float& L, float& R) {
        (this->*getSampleVariants)(L, R);
    }
    void  getSample8m(float& L, float& R);
    void  getSample8s(float& L, float& R);
    void  getSample16m(float& L, float& R);
    void  getSample16s(float& L, float& R);
    void  getSample24m(float& L, float& R);
    void  getSample24s(float& L, float& R);
    void  getSample32m(float& L, float& R);
    void  getSample32s(float& L, float& R);
    void  getFadeSample(volatile float& L, volatile float& R);
    void  start(const sample_t nextSmp, uint8_t nextNote, uint8_t nextVelo);
    void  end(Adsr::eEnd_t);
    void  fadeOut();
    void  feed();
    inline uint32_t   hunger();
    inline void       setPressed(bool pr)   {_pressed = pr;}
    inline int        getChannels()   {return _sampleFile.channels;}
    inline bool       isActive()      {return _active;}
    inline uint8_t    getMidiNote()   {return _midiNote;}
    inline uint8_t    getMidiVelo()   {return _midiVelo;}
    inline uint32_t   getBufPlayed()  {return _bufPlayed;}
    inline float      getAmplitude()  {return _amplitude;}
    inline float      getFeedScore()        ;
    inline int        getPlayPos()    {return _bufPosSmp[_idToPlay];}
    inline uint32_t   getBufSize()    {return _bufSizeSmp;}
    inline void       toggleBuf();
    inline float      interpolate(float& s1, float& s2, float i);
    
  private:
    void (Voice::*getSampleVariants)(float&, float&) = &Voice::getSample16s;
    SDMMC_FAT32*        _Card                   ;
    bool*               _sustain                ; // every voice needs to know if sustain is ON. Propagating it in a loop I thought wasn't a good idea
    volatile float      _amp                    = 1.0f;
    volatile bool       _active                 = false;
    volatile int16_t*   _buffer0;               // pointer to the 1st allocated SD-reader buffer
    volatile int16_t*   _buffer1;               // pointer to the 2nd allocated SD-reader buffer
    volatile int16_t*   _playBuf16;             // pointer to the buffer which is ready to play (one of the two allocated)
    volatile int16_t*   _fillBuf16;             // pointer to the buffer which awaits filling (one of the two allocated)
    volatile uint8_t*   _playBuf8;              // pointer to the buffer which is ready to play (one of the two allocated)
    volatile uint8_t*   _fillBuf8;              // pointer to the buffer which awaits filling (one of the two allocated)
    volatile float*     _fadeL;                 // fade out left samples to avoid click on instant retrigging voice
    volatile float*     _fadeR;                 // fade out right samples 
    uint32_t            _bufSizeBytes           = READ_BUF_SECTORS * BYTES_PER_SECTOR;
    uint32_t            _bufSizeSmp             = 0;     
    uint32_t            _hunger                 = 0;
    volatile int        _fadePoint              = FADE_OUT_SAMPLES;
    int                 _bytesToRead            = 0; // can be negative
    volatile int        _offset                 = 0; // buf offset (bytes or int16_t's depending on sample bit depth)
    volatile int        _bufCoef                = 2; // 2 for 16 bits, 1 for 24 bits
    volatile int        _samplesInBuf           = 0;
    volatile int        _bufPosSmp[2]           = {0, 0}; // sample pos, it depends on the number of channels and bit depth of a wav file assigned to this voice;
    float               _bufPosSmpF             = 0.0;    // exact calculated sample reading position including speed, pitchbend etc. 
    volatile bool       _bufEmpty[2]            = {true, true};
    uint32_t            _smpSize                = 4;      // bytes
    float               _divFileSize            = 0.001;
    float               _divVelo                = 0;
    volatile int        _idToFill               = 0;      // tic-tac buffer id
    volatile int        _idToPlay               = 1;
    uint8_t             _midiNote               = 0;
    uint8_t             _midiVelo               = 0;
    float               _speed                  = 1.0;    // _speed param corrects the central freq of a sample 
    float               _speedModifier          = 1.0;    // pitchbend, portamento etc. 
    uint32_t            _lastSectorRead         = 0;      // last sector that was read during this sample playback
    uint32_t            _curChain               = 0;      // current chain (linear non-fragmented segment of a sample file) index
    uint32_t            _bufPlayed              = 0;      // number of buffers played (for float correction)
    volatile float      _amplitude              = 0.0;
    volatile bool       _pressed                = false;
    volatile bool       _eof                    = true;
    float               _feedScoreCoef          = 1.0f;
    float               _hungerCoef             = 1.0f;
    sample_t            _sampleFile             ;
    int                 _lowest                 = 1;
    Adsr                AmpEnv                  ;
};
