#pragma once

#define MAX_VELOCITY_LAYERS 16
#define MAX_POLYPHONY 19 // empiric : MAX_POLYPHONY * READ_BUF_SECTORS <= 156
#define ROOT_FOLDER "/" // only </> is supported yet
#define WAV_CHANNELS 2
#define MAX_CONFIG_LINE_LEN 100 // 4 < x < 256 , must be divisible by 4
#define STR_LEN MAX_CONFIG_LINE_LEN
#define MAX_DISTANCE_STRETCH 3 // max distance in semitones to search for an absent sample by stretching neighbour files

#include <vector>
#include <FixedString.h>
#include "voice.h"
#include "sdmmc.h"

enum eVoiceAlloc_t  { VA_OLDEST, VA_MOST_QUIET, VA_PERCEPTUAL, VA_NUMBER }; // not all implemented
enum eVeloCurve_t   { VC_LINEAR, VC_SOFT1, VC_SOFT2, VC_SOFT3, VC_HARD1, VC_HARD2, VC_HARD3, VC_CONST, VC_NUMBER }; // not all implemented
enum eItem_t        { P_NUMBER, P_NAME, P_OCTAVE, P_SEPARATOR, P_VELO }; 

using short_t = FixedString<8>; 
using str_t = FixedString<STR_LEN>;

typedef struct {
  short_t     name[2];            // sharp (#) and flat (b) variants
  int         octave      ;       // octave number (-1 to 9)
  uint8_t     velo_layer  ;       // velocity layer that sample belongs to
  float       freq        ;       // calculated frequency for each midi note (A4=440Hz)
  float       tuning      = 1.0f; // 0.9 = 10% lower, 1.1 = 10% higher
  int         transpose   = 0;    // +/- semitones
} midikey_t;

typedef struct {
  eItem_t               item_type;
  FixedString<8>        item_str;
  std::vector<short_t>  item_variants;
} template_item_t;

typedef struct {
  int   midi_note;
  int   velo_layer;
} parsed_t;

class SamplerEngine {
  
  public:
    SamplerEngine() {}; 
    void            init(SDMMC_FAT32* Card);
    void            initKeyboard();
    void            getSample(float& sampleL, float& sampleR);
    fname_t         getFolderName(int id)                 { return _folders[id]; }
    fname_t         getCurrentFolder()                    { return _currentFolder; }
    int             getActiveVoices();
    int             scanRootFolder();                       // scans root folder for sample directories, returns count of valid sample folders, -1 if error
    void            loadInstrument(uint8_t noteLow=0, uint8_t noteHigh=127); // loads config and prepares samples from the current folder
    inline void     setSampleRate(unsigned int sr)        { if (sr > 1) _sampleRate = sr; }
    inline void     setRootFolder(const fname_t& rf)      { _rootFolder = rf; }
    inline void     setMaxVoices(byte mv)                 { _maxVoices = constrain(mv, 1, MAX_POLYPHONY); }
    inline void     setVoiceAllocMethod(eVoiceAlloc_t va) { _voiceAllocMethod = va ; }
    inline void     setCurrentFolder(int folder_id);        // sets current folder to the desired path[folder_id]
    inline void     setNextFolder();                        // sets current folder to the next dir which was found during scanFolders()
    inline void     setPrevFolder();                        // sets current folder to the previous dir which was found during scanFolders()
    inline void     setSustain(bool onoff)                ;
    inline void     noteOn(uint8_t midiNote, uint8_t velocity);
    inline void     noteOff(uint8_t midiNote, bool hard = false);
    void            fillBuffer();
    
  private:
    SDMMC_FAT32*    _Card;
    inline int      assignVoice(byte midi_note, byte midi_velocity);    // returns id of a slot to use for a new note
    void            parseIni();                  // loads config from current folder, determining how wav files spread over the notes/velocities
    bool            parseMelodicTemplate(str_t& line);
    void            parserGetLine(char* buf, str_t& str) ;
    parsed_t        processMeloParser(fname_t& fname);
    void            parseBoolValue(bool& var, str_t& val);
    void            parseWavHeader(entry_t* entry, sample_t& smp);
    void            finalizeMapping();
    void            buildVeloCurve();
    uint8_t         mapVelo(uint8_t velo);
    uint8_t         unMapVelo(uint8_t mappedVelo);
    void            resetSamples();
    static constexpr float DIV128 = 1.0f / 128.0f;
    sample_t        _sampleMap[128][MAX_VELOCITY_LAYERS];
    midikey_t       _keyboard[128];
    float           _ampCurve[128];       // velocity to amplification mapping [0.0 ... 1.0] to make seamless velocity response curve
    eVeloCurve_t    _veloCurve            = VC_LINEAR;
    uint8_t         _veloLayers           = 1;
    float           _divVeloLayers        = 1.0;
    int             _sampleRate           = SAMPLE_RATE;
    byte            _sampleChannels       = WAV_CHANNELS; // 2 = stereo, 1 = mono : use or not stereo data in sample files
    byte            _maxVoices            = MAX_POLYPHONY; 
    fname_t         _rootFolder           ;
    volatile int    _currentFolderId      = 0;
    fname_t         _currentFolder        ;
    int             _sampleSetsCount      = 0;
    eVoiceAlloc_t   _voiceAllocMethod     = VA_OLDEST;
    int             _parser_i             = 0;
    bool            _enveloped            = true;
    bool            _normalized           = false;
    bool            _sustain              = false;
    Voice           Voices[MAX_POLYPHONY] ;
    std::vector<fname_t>         _folders ;
    std::vector<template_item_t> _melo_template;
};
