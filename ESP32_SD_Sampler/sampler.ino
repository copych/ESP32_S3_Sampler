#include "sampler.h"
#include "sdmmc_file.h"

void SamplerEngine::init(SDMMC_FAT32* Card){
  _Card = Card;
  _rootFolder = ROOT_FOLDER;
  _limitSameNotes = MAX_SAME_NOTES;
  _maxVoices = MAX_POLYPHONY;
  int num_sets = scanRootFolder();
  for (int i = 0; i < num_sets; i++) {
    DEBF("Folder %d : %s\r\n" , i ,_folders[i].c_str());
  }
  DEBF("Total %d folders with samples found\r\n", num_sets);
  initKeyboard();
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    DEBF("Voice %d: ", i);
    // sustain is global and needed for every voice, so we just pass a pointer to it.
    Voices[i].init(Card, &_sustain, &_normalized);
    Voices[i].my_id = i;
  }
  if (num_sets > 0) {
    setSampleRate(SAMPLE_RATE);
    DEBUG("+++");
  } else {
    while(true) {
      // loop forever
      delay(1000);
      DEBUG("--- no samples found");
    }      
  }
  setReverbSendLevel(0.2f);
  setDelaySendLevel(0.0f);
}

int SamplerEngine::scanRootFolder() {  
  fpath_t dirname;
  DEBUG("SAMPLER: Scanning root folder");
  SDMMC_FileReader Reader(_Card);
  _rootFolder = ROOT_FOLDER;
  _folders.clear();
  _Card->setCurrentDir(_rootFolder);
  int index = 0;
  while (true) {
    entry_t* entry = _Card->nextEntry();
    if (entry->is_end) break;
    if (entry->is_dir) {
      point_t p = _Card->getCurrentPoint();
      dirname = entry->name;      
      DEBUG(dirname.c_str());
      _Card->setCurrentDir(dirname);
      if (Reader.open(INI_FILE) == ESP_OK ) {
        Reader.close();
        _folders.push_back(dirname);
        index++;
      }
      _Card->setCurrentPoint(p);
    }
  }
  _sampleSetsCount = index;
  return index;
}

inline int SamplerEngine::assignVoice(byte midi_note, byte velo){
  float maxVictimScore = 0.0;
  float minAmp = 1.0e30;
  int id = 0;
  
  for (int i = 0 ; i < _maxVoices ; i++) {
    if (!Voices[i].isActive()){
   //   DEBUG("SAMPLER: First vacant voice");
      return (int)i;
    }
  }  

  for (int i = 0 ; i < _maxVoices ; i++) {
    if (Voices[i].getKillScore() > maxVictimScore){
      maxVictimScore = Voices[i].getKillScore();
      id = i;
    }
  }
  DEBUG("SAMPLER: No free slot: Steal a voice");
  return id;
}

inline void SamplerEngine::noteOn(uint8_t midiNote, uint8_t velo){
  int i = assignVoice(midiNote, velo);
  sample_t smp = _sampleMap[midiNote][mapVelo(velo)];
  if (smp.channels > 0) {
   // DEBF("SAMPLER: voice %d note %d velo %d\r\n", i, midiNote, velo);
    Voices[i].setStarted(false);
    Voices[i].start(_sampleMap[midiNote][mapVelo(velo)], midiNote, velo);
  } else {
    DEBUG("SAMPLER: no sample assigned");
    return;
  }
}

inline void SamplerEngine::noteOff(uint8_t midiNote, bool hard ){
  Adsr::eEnd_t end_type ;
  if (hard) end_type = Adsr::END_NOW; else end_type = Adsr::END_REGULAR;
  if (_keyboard[midiNote].noteoff || hard) {
    for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
      if (Voices[i].getMidiNote() == midiNote && Voices[i].isActive()) {      
        // DEBF("SAMPLER: NOTE OFF Voice %d note %d \r\n", i, midiNote);
        Voices[i].setPressed(false);
        Voices[i].end(end_type);
        // return;
      }
    }
  }
}


inline void SamplerEngine::setSustain(bool onoff) {
  _sustain = onoff; 
  DEBF("SAMPLER: sustain: %d\r\n", onoff);
  if (!onoff) {
    for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
      if (_keyboard[Voices[i].getMidiNote()].noteoff && Voices[i].isActive() ) {
        Voices[i].end(Adsr::END_REGULAR);   
      }
    }
  }
}




void SamplerEngine::setAttackTime(float val) {
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j < 128; j++ ) {
      _sampleMap[j][i].attack_time = val;
    }
  }
  for (int i = 0; i < _maxVoices; i++) {
    Voices[i].setAttackTime(val);    
  }
}

void SamplerEngine::setDecayTime(float val) {
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j < 128; j++ ) {
      _sampleMap[j][i].decay_time = val;
    }
  }
  for (int i = 0; i < _maxVoices; i++) {
    Voices[i].setDecayTime(val);    
  }
}

void SamplerEngine::setReleaseTime(float val) {
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j < 128; j++ ) {
      _sampleMap[j][i].release_time = val;
    }
  }
  for (int i = 0; i < _maxVoices; i++) {
    Voices[i].setReleaseTime(val);    
  }
}

void SamplerEngine::setSustainLevel(float val) {
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j < 128; j++ ) {
      _sampleMap[j][i].sustain_level = val;
    }
  }
  for (int i = 0; i < _maxVoices; i++) {
    Voices[i].setSustainLevel(val);    
  }
}


uint8_t SamplerEngine::mapVelo(uint8_t velo) {
  switch(_veloCurve) {
    case VC_LINEAR:
      return (uint8_t)((float)velo * (float)_veloLayers * DIV128);
    default: 
      return _veloLayers-1;
    return 0;
  }
}

uint8_t SamplerEngine::unMapVelo(uint8_t mappedVelo) {
  switch(_veloCurve) {
    case VC_LINEAR:
      return (uint8_t)(127.0 * (float)mappedVelo * _divVeloLayers) ;
    default: 
      return 90;
    return 0;
  }
}

void IRAM_ATTR SamplerEngine::fillBuffer() {
  // search and fill the most hungry buffer
  static uint32_t hungerMax;
  static uint32_t hunger;
  static int iToFeed;
  hunger = hungerMax = 0;
  iToFeed = 0;
  for (int i=0; i<_maxVoices; i++) {
    hunger = Voices[i].hunger();
    if (hunger > hungerMax) {
      hungerMax = hunger;
      iToFeed = i;
    }
  }
  Voices[iToFeed].feed(); 
}


inline void SamplerEngine::setCurrentFolder(int folder_id) {
  resetSamples();
  folder_id = constrain(folder_id, 0, _sampleSetsCount-1); // just in case
  _currentFolder = _folders[folder_id];
  _currentFolderId = folder_id;
  _Card->setCurrentDir(_rootFolder);
  DEB(folder_id);
  DEB(": ");
  DEBUG(_folders[folder_id].c_str());
  _Card->setCurrentDir(_folders[folder_id]);
  initKeyboard();               // it resets _keyboard[] which holds key-specific parameters
  parseIni();                   // this will read the sampler.ini file and prepare name template along with other parameters
  _Card->rewindDir();
  while (true) {                // iterate thru the selected directory
    entry_t* entry = _Card->nextEntry();
    if (entry->is_end) break;
    if (!entry->is_dir) {
      processNameParser(entry); // parse filenames basing on a prepared template
    }
  }
  printMapping();
  finalizeMapping();  // fill the gaps when we don't have dedicated samples for some pitches or velocity layers
  printMapping();
}


inline void SamplerEngine::setNextFolder() {
  _currentFolderId++;
  if (_currentFolderId > _sampleSetsCount-1) _currentFolderId = 0;
  setCurrentFolder(_currentFolderId);
}

inline void SamplerEngine::setPrevFolder() {
  _currentFolderId--;
  if (_currentFolderId < 0) _currentFolderId = _sampleSetsCount - 1;
  setCurrentFolder(_currentFolderId);
}

void SamplerEngine::initKeyboard() {
  for (int i=0; i<128; ++i) {
    _keyboard[i].freq         = (440.0f / 32.0f) * pow(2, ((float)(i - 9) / 12.0f));
    _keyboard[i].octave       = (i / 12) -1;
    _keyboard[i].name[0]      = notes[0][i%12];
    _keyboard[i].name[1]      = notes[1][i%12];
    _keyboard[i].transpose    = 0;
    _keyboard[i].noteoff      = true;
    //_keyboard[i].velo_layer   = 1;
    _keyboard[i].tuning       = 1.0f;
    // DEBF("%d:\t%s\t%s\t%d\t%7.3f\r\n", i, _keyboard[i].name[0].c_str(), _keyboard[i].name[1].c_str(), _keyboard[i].octave, _keyboard[i].freq);
  }
}

void SamplerEngine::resetSamples() {
  _amp = 1.0;
  _attackTime   = 0.0f;
  _decayTime    = 0.1f;
  _sustainLevel = 1.0f;
  _releaseTime  = 8.0f;
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    Voices[i].end(Adsr::END_FAST);    
  }
  for (int i = 0; i<MAX_VELOCITY_LAYERS; i++) {
    for (int j = 0; j<128; j++) {
      _sampleMap[j][i].sectors.clear();
      _sampleMap[j][i].byte_offset = 44;
      _sampleMap[j][i].size = 0;      
      _sampleMap[j][i].sample_rate   = 44100;
      _sampleMap[j][i].channels   = 0;
      _sampleMap[j][i].orig_freq = _keyboard[j].freq;
      _sampleMap[j][i].orig_velo_layer = 0;
      _sampleMap[j][i].amp = 1.0;
      _sampleMap[j][i].speed = 0.0f;
      _sampleMap[j][i].bit_depth = 0;
      _sampleMap[j][i].attack_time   = _attackTime;
      _sampleMap[j][i].decay_time    = _decayTime;
      _sampleMap[j][i].sustain_level = _sustainLevel;
      _sampleMap[j][i].release_time  = _releaseTime;
      _sampleMap[j][i].native_freq  = false;
    }
  }
}

int SamplerEngine::getActiveVoices() {
  int n=0;
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].isActive()) n++;    
  }
  return n;
}

void SamplerEngine::getSample(float& sampleL, float& sampleR){
  float sL = 0.0f;
  float sR = 0.0f;
  sampleL = 0.0f;
  sampleR = 0.0f;
  for (int i = 0; i < _maxVoices; i++) {
    Voices[i].getSample(sL, sR);
    sampleL = sampleL + sL;
    sampleR = sampleR + sR;    
  }  
}

void SamplerEngine::freeSomeVoices() {
  int id, n=0;
  static byte note_count[128];
  int midi_note;
  int desiredFree = SACRIFY_VOICES;
  float score, maxKillScore = 0.0f;
  memset(note_count, 0, 128);
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].isActive() && !Voices[i].isDying() ) {
      n++;
      midi_note = Voices[i].getMidiNote();
      note_count[midi_note]++;
      if (note_count[midi_note] > _limitSameNotes) { // if we have limit overrun, find the best candidate
        for (int j = 0 ; j < MAX_POLYPHONY ; j++) {
          if (Voices[j].getMidiNote() == midi_note) {
            score = Voices[j].getKillScore();
            if (score > maxKillScore) {
              maxKillScore = score;
              id = j;
            }
          }
        }
        Voices[id].end(Adsr::END_FAST);
        return;
      }
    }
    score = Voices[i].getKillScore();
    if (score > maxKillScore) {
      maxKillScore = score;
      id = i;
    }
  }
  if (MAX_POLYPHONY < n + desiredFree) {
    Voices[id].end(Adsr::END_FAST);
    return;
  }
}
