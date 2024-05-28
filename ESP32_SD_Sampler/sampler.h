#pragma once

#define MAX_VELOCITY_LAYERS   16
#define MAX_SAME_NOTES        3
#define MAX_POLYPHONY         18         // empiric : MAX_POLYPHONY * READ_BUF_SECTORS <= 156
#define ROOT_FOLDER           "/"         // only </> is supported yet
#define WAV_CHANNELS          2
#define MAX_CONFIG_LINE_LEN   256         // 4 < x < 256 , must be divisible by 4
#define MAX_DISTANCE_STRETCH  2           // max distance in semitones to search for an absent sample by stretching neighbour files
#define STR_LEN               MAX_CONFIG_LINE_LEN

#include <vector>
#include <FixedString.h>
#include "voice.h"
#include "sdmmc.h"

enum eVoiceAlloc_t  { VA_OLDEST, VA_MOST_QUIET, VA_PERCEPTUAL, VA_NUMBER }; // not implemented
enum eVeloCurve_t   { VC_LINEAR, VC_SOFT1, VC_SOFT2, VC_SOFT3, VC_HARD1, VC_HARD2, VC_HARD3, VC_CONST, VC_NUMBER }; // not implemented
enum eItem_t        { P_NUMBER, P_NAME, P_OCTAVE, P_SEPARATOR, P_VELO, P_INSTRUMENT }; // filename template elements 
enum eInstr_t       { SMP_MELODIC, SMP_PERCUSSIVE }; 
enum eSection_t     { S_NONE, S_SAMPLESET, S_FILENAME, S_ENVELOPE, S_NOTE, S_RANGE, S_GROUP };

using str8_t    = FixedString<8>; 
using str20_t   = FixedString<20>;
using str64_t   = FixedString<64>;
using str256_t  = FixedString<STR_LEN>;
using variants_t = std::vector<str20_t>;

typedef struct {
  uint8_t     first  = 0     ;       // lowest midi note in range
  uint8_t     last   = 127     ;       // highest midi note in range
  str20_t     instr       = "";
  bool        noteoff     = true;
  float       speed       = 1.0f;
  int         velo_layer  = -1;
  
  float       attack_time   = 0.0f;
  float       decay_time    = 0.5f;
  float       sustain_level = 1.0f;
  float       release_time  = 12.0f;
  bool        loop          = false;
  inline void clear(eInstr_t t) {
    first = 0;
    last = 127;
    instr = "";
    velo_layer = -1;
    noteoff = (t != SMP_PERCUSSIVE) ;
    speed = 1.0f;
  }
} ini_range_t;

typedef struct {
  str8_t      name[2]     ;       // sharp (#) and flat (b) variants
  int         octave      ;       // octave number (-1 to 9)
//  uint8_t     velo_layer  ;       // velocity layer that sample belongs to
  float       freq        ;       // calculated frequency for each midi note (A4=440Hz)
  float       tuning      = 1.0f; // 0.9 = 10% lower, 1.1 = 10% higher
  int         transpose   = 0;    // +/- semitones
  bool        noteoff     = true; // should it handle note off messages
  int         group       = -1;  
  float       attack_time   = 0.0f;
  float       decay_time    = 0.5f;
  float       sustain_level = 1.0f;
  float       release_time  = 12.0f;
} midikey_t;

typedef struct {
  eItem_t                item_type;
  str8_t                 item_str;
} template_item_t;

/*
typedef struct {
  int      midi_note;
  str20_t  instr;
} mapping_t;
*/

const str8_t notes[2][12]= {
  {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},
  {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"}
}; 
  
class SamplerEngine {
  
  public:
    SamplerEngine() {}; 
    void            init(SDMMC_FAT32* Card);
    void            initKeyboard();
    void            fadeOut(int id);
    void            getSample(float& sampleL, float& sampleR);
    fname_t         getFolderName(int id)                 { return _folders[id]; }
    fname_t         getCurrentFolder()                    { return _currentFolder; }
    int             getActiveVoices();
    void            freeSomeVoices();
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
    bool            parseFilenameTemplate(str256_t& line);
    void            processNameParser(entry_t* entry);
    eSection_t      parseSection( str256_t& val );
    bool            parseBoolValue( str256_t& val );
    float           parseFloatValue( str256_t& val );
    int             parseIntValue( str256_t& val );
    eInstr_t        parseInstrType( str256_t& val );
    variants_t      parseVariants( str256_t& val); 
    void            parseWavHeader(entry_t* entry, sample_t& smp);
    void            applyRange(ini_range_t& range);
    void            finalizeMapping();
    void            buildVeloCurve();
    uint8_t         mapVelo(uint8_t velo);
    uint8_t         unMapVelo(uint8_t mappedVelo);
    void            resetSamples();
    uint8_t         midiNoteByName(str8_t noteName);
    void            printMapping();
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
    int             _limitSameNotes       = MAX_SAME_NOTES;
    fname_t         _rootFolder           ;
    volatile int    _currentFolderId      = 0;
    fname_t         _currentFolder        ;
    int             _sampleSetsCount      = 0;
    float           _amp                  = 1.0;
    float           _attackTime           = 0.0;
    float           _decayTime            = 0.05;
    float           _releaseTime          = 8.0;
    float           _sustainLevel         = 1.0;
    eVoiceAlloc_t   _voiceAllocMethod     = VA_OLDEST; // not implemented, VA_PERCEPTUAL is gonna be the only one
    int             _parser_i             = 0;
    bool            _enveloped            = true;
    bool            _normalized           = false;
    bool            _sustain              = false;
    eInstr_t        _type                 = SMP_MELODIC;
    str64_t         _title                = "";
    Voice           Voices[MAX_POLYPHONY]  ;
    variants_t      _veloVars              ;
    std::vector<fname_t>          _folders ;
    std::vector<template_item_t>  _template;
    std::vector<ini_range_t>      _ranges  ;
};
