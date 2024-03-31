#include "sampler.h"

void SamplerEngine::init(SDMMC_FAT32* Card){
  _Card = Card;
  _rootFolder = ROOT_FOLDER;
  int num_sets = scanRootFolder();
  for (int i = 0; i < num_sets; i++) {
    DEBF("Folder %d : %s\r\n" , i ,_folders[i].c_str());
  }
  DEBF("Total %d folders with samples found\r\n", num_sets);
  initKeyboard();
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    DEBF("Voice %d: ", i);
    // sustain is global and needed for every voice, so we just pass a pointer to it.
    Voices[i].init(Card, &_sustain);
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
}

int SamplerEngine::scanRootFolder() {
  _rootFolder = ROOT_FOLDER;
  _folders.clear();
  _Card->setCurrentDir(_rootFolder);
  int index = 0;
  fname_t str;  
  while (true) {
    entry_t* entry = _Card->nextEntry();
    if (entry->is_end) break;
    if (entry->is_dir) {
      _folders.push_back(entry->name);
      index++;
    }
  }
  _sampleSetsCount = index;
  return index;
}

inline int SamplerEngine::assignVoice(byte midi_note, byte velo){
  float maxAge = 0.0;
  float maxAmp = 0.0;
  int id = 0;
  /*
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].getMidiNote() == midi_note){
      Voices[i].end(false);
      DEBUG("SAMPLER: Retrig same note");
      return (int)i;
    }
  }
  */
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (!Voices[i].isActive()){
   //   DEBUG("SAMPLER: First vacant voice");
      return (int)i;
    }
  }  
  /*
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].getAmplitude() > maxAmp){
      maxAmp = Voices[i].getAmplitude();
      id = i;
     // Voices[i].end(false);
    }
  }
  DEBUG("Retrig most quiet");
  return id;
  */
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].getAge() > maxAge){
      maxAge = Voices[i].getAge();
      id = i;
     // Voices[i].end(false);
    }
  }
  DEBUG("SAMPLER: Retrig oldest");
  return id;
}

inline void SamplerEngine::noteOn(uint8_t midiNote, uint8_t velo){
  int i = assignVoice(midiNote, velo);
  sample_t smp = _sampleMap[midiNote][mapVelo(velo)];
  if (smp.channels > 0) {
    Voices[i].start(_sampleMap[midiNote][mapVelo(velo)], midiNote, velo);
    // DEBF("SAMPLER: voice %d note %d velo %d\r\n", i, midiNote, velo);
  } else {
    return;
  }
}

inline void SamplerEngine::noteOff(uint8_t midiNote, bool hard ){
  for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
    if (Voices[i].getMidiNote() == midiNote) {      
      // DEBF("SAMPLER: NOTE OFF Voice %d note %d \r\n", i, midiNote);
      Voices[i].setPressed(false);
      Voices[i].end(hard);
      // return;
    }
  }
}


inline void SamplerEngine::setSustain(bool onoff) {
  _sustain = onoff; 
  DEBF("SAMPLER: sustain: %d\r\n", onoff);
  if (!onoff) {
    for (int i = 0 ; i < MAX_POLYPHONY ; i++) {
      Voices[i].end(false);   
    }
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
  for (int i=0; i<MAX_POLYPHONY; i++) {
    hunger = Voices[i].hunger();
    if (hunger > hungerMax) {
      hungerMax = hunger;
      iToFeed = i;
    }
  }
  //DEBF("Feed voice #%d\r\n" , iToFeed);
  Voices[iToFeed].feed(); 
}
    
inline void SamplerEngine::setCurrentFolder(int folder_id) {
  resetSamples();
  folder_id = constrain(folder_id, 0, _sampleSetsCount-1); // just in case
  _currentFolder = _folders[folder_id];
  _currentFolderId = folder_id;
  _Card->setCurrentDir("");
  DEBUG(folder_id);
  DEBUG(_folders[folder_id].c_str());
  _Card->setCurrentDir(_folders[folder_id]);
  parseIni();
  _Card->rewindDir();
  while (true) {
    entry_t* entry = _Card->nextEntry();
    if (entry->is_end) break;
    if (!entry->is_dir) {
      fname_t fname = entry->name;
      parsed_t rcv = processMeloParser(fname);
      if (rcv.midi_note >= 0  &&  rcv.velo_layer >= 0) {
        sample_t smp;
        smp.orig_velo_layer = rcv.velo_layer;
        smp.sectors = entry->sectors;
        smp.size = entry->size;
        parseWavHeader(entry, smp);
        _sampleMap[rcv.midi_note][rcv.velo_layer] = smp;
      }
    }
  }
  finalizeMapping();
  const short_t notes[2][12]= {
    {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},
    {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"}
  };
  for (int i = 0; i < 128; i++) {
   //   DEBF("%s%d\t", notes[1][i%12].c_str(), i/12 -1 );
  }
  //DEBUG("");
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j<128; j++ ) {
//      DEBF("%0.4f\t", _sampleMap[j][i].speed );
    }
    //DEBUG("");
  }
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

void SamplerEngine::parseIni() {
  _melo_template.clear();
  _Card->rewindDir();
  int i, a, b;
  str_t iniStr;
  // read the first sector of a sampler.ini into a char array
  _parser_i = 0;
  DEBUG("Reading ini file");
  char* tpl = reinterpret_cast<char*>(_Card->readFirstSector(INI_FILE));
  while (true) {
    parserGetLine(tpl, iniStr);
  //DEBUG(iniStr.c_str()); 
    iniStr.toUpperCase();
    if (iniStr.empty()) break;
    if (iniStr.startsWith("#")) continue;
    int s = iniStr.indexOf('=');
    if (s==0) continue;
    str_t val = iniStr.substring(s+1);
    if (iniStr.substring(0, s) == "MELODIC") parseMelodicTemplate(val) ;
    if (iniStr.substring(0, s) == "NORMALIZED") parseBoolValue(_normalized, val) ;
    if (iniStr.substring(0, s) == "ENVELOPED") parseBoolValue(_enveloped, val) ;
  }
}

void SamplerEngine::parseBoolValue(bool& var, str_t& val) {
  val.trim();
  val.toUpperCase();
  var = false;
  if (val=="TRUE" || val=="1") var = true;
  if (val=="FALSE" || val=="0") var = false;
}

bool SamplerEngine::parseMelodicTemplate(str_t& line) {
  DEBF("Parse melodic template:[[[%s]]]\r\n", line.c_str());
  int a, b, i;
  str_t str;
  template_item_t item;
  i = 0;
  while (true) {
    a = line.indexOf("<",i);
    b = line.indexOf(">",i);
    if (a < 0  ||  b < 0) break;
    if (b < a) return false;
    if (a > i && b > a) {  // at the start of separator
      str = line.substring(i, a );
      DEBF ("Parsing melodic template: String separator: [%s]\r\n", str.c_str());
      item.item_type = P_SEPARATOR;
      item.item_str = str;
      item.item_variants.clear();
      _melo_template.push_back(item);
      i = a;
    }
    if (a == i) { // at the start of token
      str = line.substring(a + 1, b );
      DEBF ("Parsing melodic template: Token: [%s]\r\n", str.c_str());
      str = str.substring(0,4);
      if (str ==  "NUMB" ) {
          item.item_type = P_NUMBER;
          item.item_str = "";
          item.item_variants.clear();
          _melo_template.push_back(item);
      }
      else if (str == "NAME" ) {
          item.item_type = P_NAME;
          item.item_str = "";
          item.item_variants.clear();
          _melo_template.push_back(item);
      }
      else if (str == "OCTA" ) {
          item.item_type = P_OCTAVE;
          item.item_str = "";
          item.item_variants.clear();
          _melo_template.push_back(item);
      }
      else if (str == "VELO" ) {
          item.item_type = P_VELO;
          item.item_str = "";
          item.item_variants.clear();
          int s = line.indexOf(":");
          if (s<0) return false;
          str = line.substring(s + 1, b );
          DEBF("Values set: <%s>\r\n" , str.c_str());
          int j = 0;
          int k = 0;
          while (true) {
            short_t s1;
            k = str.indexOf("," , j);
            if (k == -1 || k < j) k = str.length();
            s1 = str.substring(j, k);
      
            item.item_variants.push_back(s1);
            if (k == str.length()) break;
            j = k + 1;
          }
          _melo_template.push_back(item);    
      }
    }
    i = b + 1;
    if (i >= line.length()) return true;
  }
  return true;
}

void SamplerEngine::parserGetLine(char* buf, str_t& str) {
  str = "";
  for (; _parser_i<STR_LEN  ; _parser_i++) {
    if (buf[_parser_i]=='\0' || buf[_parser_i]=='\r' || buf[_parser_i] == '\n') {
      str += '\0';
      break;
    }
    str += buf[_parser_i];
  }
  str.trim();
}

parsed_t SamplerEngine::processMeloParser(fname_t& fname) {
  int pos = 0;
  int i = 0;
  int len = 0;
  int oct = -2;
  int velo = 0;
  int note_num = -1;
  int note_num1 = -1;
  int note_num2 = -1;
  int match_weight = 0;
  short_t s;
  sample_t smp;
  
  fname.toUpperCase();
  if (fname.endsWith(".WAV")) {
    // DEBF("Parsing name: <%s>\r\n", fname.c_str());
  } else {
    DEBF("Skipping name: <%s>\r\n", fname.c_str());
    return {-1,-1};
  }
  const short_t notes[2][12]= {
    {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},
    {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"}
  };
  for (auto tpl: _melo_template) {
    switch ((int)tpl.item_type) {
      case P_NAME:
        match_weight = 0;
        for (int j = 0; j < 2 ; j++) {
          for (int k = 0 ; k < 12; k++) {
            short_t a = notes[j][k];
            len = a.length();
            short_t b = fname.substring(pos, pos + len);
          //  DEBF("compare: <%s> <%s>\r\n", a.c_str() , b.c_str());
            if (a.equalsIgnoreCase(b) && len > match_weight) {
              match_weight = len;
              note_num = k;
            }
          }
        }
        if ( note_num >= 0 ) {
          pos += match_weight;
        }
        else {
          DEBF("Failed to determine note name in <%s>\r\n", fname.c_str());
        }
//        DEBF("Note: %s\r\n", notes[0][note_num].c_str());
        break;
      case P_NUMBER:
        s = "";
        while (true) {
          if (fname.charAt(pos) >='0' && fname.charAt(pos) <='9' ) {
            s += fname.charAt(pos);
            pos++;
          } else {
//            DEBF("Number: %s\r\n", s.c_str());
            break;
          }
        }
        break;
      case P_OCTAVE:
        s = fname.substring(pos,pos+1);
        oct = s.toInt();
//        DEBF("Octave: %d\r\n" , oct);
        pos++;
        break;
      case P_VELO:
        i = 0;
        match_weight = 0;
        for(auto var: tpl.item_variants) {
          len = var.length();
          s = fname.substring(pos, pos + len);
          if (len>match_weight && s.equalsIgnoreCase(var)) {
            match_weight = len;
            velo = i;
          }
          i++;
        }
//        DEBF("Velocity: %d\r\n", velo);
        pos += match_weight;
        break;
      case P_SEPARATOR:
        s = tpl.item_str;
        len = s.length();
//        DEBF("Skipping separator %s\r\n", s.c_str());
        pos += len;
        break;
      default:
        //impossible:
      break;
    }
  }
  
  if (note_num < 0) return {-1,-1};
  if (oct < -1) return {-1,-1};
  return {(note_num+(oct+1)*12), velo};
}

void SamplerEngine::parseWavHeader(entry_t* wav_entry, sample_t& smp){
  char* buf = reinterpret_cast<char*>(_Card->readFirstSector(wav_entry));
  wav_header_t* wav = reinterpret_cast<wav_header_t*>(buf);

  smp.sample_rate = wav->sampleRate;
  smp.bit_depth = wav->bitsPerSample;
  smp.channels = wav->numberOfChannels;
  smp.speed = (float)(wav->sampleRate) * DIV_SAMPLE_RATE;
}

void SamplerEngine::initKeyboard() {
  const short_t notes[2][12]= {
    {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},
    {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"}
  };
  for (int i=0; i<128; ++i) {
    _keyboard[i].freq         = (440.0f / 32.0f) * pow(2, ((float)(i - 9) / 12.0f));
    _keyboard[i].octave       = (i / 12) -1;
    _keyboard[i].name[0]     = notes[0][i%12];
    _keyboard[i].name[1]     = notes[1][i%12];
    _keyboard[i].transpose    = 0;
    _keyboard[i].velo_layer  = 1;
    _keyboard[i].tuning       = 1.0f;
   // DEBF("%d:\t%s\t%s\t%d\t%7.3f\r\n", i, _keyboard[i].name[0].c_str(), _keyboard[i].name[1].c_str(), _keyboard[i].octave, _keyboard[i].freq);
  }
}

// fill gaps
void SamplerEngine::finalizeMapping() {
  int x, y, dx, dy, xc, yc, s;
  int n = 1 + 2 * MAX_DISTANCE_STRETCH;
  _veloLayers = 1;
  // first pass: determine the number of velocity layers used
  for (int i = 0; i < MAX_VELOCITY_LAYERS; i++) {
    for (int j = 0; j < 128; j++) {
      if (_sampleMap[j][i].bit_depth > 0) {
        _veloLayers = i + 1;
        break;
      }
    }
  }
  _divVeloLayers = 1.0f / (float)_veloLayers;
  // second pass
  for (int i = 0; i < _veloLayers; i++) {
    // going up
    for (int j = 0; j < 128; j++) {
      if (_sampleMap[j][i].speed == 0.0) {
        // search in _sampleMap by spiral
        s = -1;
        x = j;
        y = unMapVelo(i) ;
        for (int k = 1; k <= n; k++) {
          dx = s;
          dy = 0;
          for (int m = 0 ; m < k; m++) {
            x += dx;
            y += dy;
            xc = constrain(x, 0, 127);
            yc = mapVelo(constrain(y, 0, 127));
            if (_sampleMap[xc][yc].speed > 0.0 ) {
              _sampleMap[j][i] = _sampleMap[xc][yc];
              _sampleMap[j][i].speed = _sampleMap[xc][yc].speed * _keyboard[j].freq / _keyboard[x].freq;
              break;  
            }
          }        
          s = -s;
          dx = 0;
          dy = s;  
          for (int m = 0 ; m < k; m++) {
            x += dx;
            y += dy;
            xc = constrain(x, 0, 127);
            yc = mapVelo(constrain(y, 0, 127));
            if (_sampleMap[xc][yc].speed == 1.0) {
              _sampleMap[j][i] = _sampleMap[xc][yc];
              _sampleMap[j][i].speed = _keyboard[j].freq / _keyboard[x].freq;
              break;  
            }
          } 
        }
      }
    }
  }
  // VC_LINEAR, VC_SOFT1, VC_SOFT2, VC_SOFT3, VC_HARD1, VC_HARD2, VC_HARD3, VC_CONST, VC_NUMBER
}

void SamplerEngine::resetSamples() {
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
      _sampleMap[j][i].speed = 0.0;
      _sampleMap[j][i].bit_depth = 0;
      _sampleMap[j][i].attack_time   = 0.01f;
      _sampleMap[j][i].decay_time    = 0.1f;
      _sampleMap[j][i].release_time  = 2.0f;
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
  float sL = 0.0;
  float sR = 0.0;
  sampleL = 0.0;
  sampleR = 0.0;
  for (int i = 0; i < MAX_POLYPHONY; i++) {
    Voices[i].getSample(sL, sR);
    sampleL += sL;
    sampleR += sR;
  }
}
