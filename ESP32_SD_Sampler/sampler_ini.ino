#include "sampler.h"
#include "sdmmc_file.h"

void SamplerEngine::parseIni() {
  eSection_t section = S_NONE;
  _veloCurve = VC_LINEAR;
  ini_range_t range;
  _template.clear();
  _ranges.clear();
  // _mapping.clear();
  _Card->rewindDir();
  int i, a, b;
  str256_t iniStr;
  str20_t tok;
  _parser_i = 0;
  DEBUG("Reading ini file");
  SDMMC_FileReader Reader(_Card);
  Reader.open(INI_FILE);
  while (Reader.available()) {
    Reader.read_line(iniStr);
    DEBUG(iniStr.c_str()); 
    iniStr.trim();
    iniStr.toUpperCase();
    if (iniStr.empty()) continue;
    else if (iniStr.startsWith("#")) continue;                      // comment
    else if (iniStr.startsWith(";")) continue;                      // comment
    if (iniStr.startsWith("[") && iniStr.endsWith("]")) {           // new section
      if (section==S_NOTE || section==S_RANGE) applyRange(range);   // save parsed range/note section
      range.clear(_type);
      section = parseSection(iniStr);
      //DEBUG(section);
      continue;
    }
    int s1 = iniStr.indexOf('='); // leftmost of ":" or "=" will be treated as left and right parts delimeter
    int s2 = iniStr.indexOf(':');
    if ( s1<=0 && s2<=0 ) continue;
    if ( s1>0 && s2>0 ) {
      if ( s1<s2 ) {
        tok = iniStr.substring(0, s1);
        iniStr.remove(0, s1+1);
      } else {
        tok = iniStr.substring(0, s2);
        iniStr.remove(0, s2+1);
      } 
    }
    if ( s2<=0 ) { // s1>0
      tok = iniStr.substring(0, s1);
      iniStr.remove(0, s1+1);
    }
    if ( s1<=0 ) { // s2>0
      tok = iniStr.substring(0, s2);
      iniStr.remove(0, s2+1);
    } 
    tok.trim();
    iniStr.trim();    
    switch (section) {
      case S_FILENAME:
        // template
        if (tok == "TEMPLATE") { parseFilenameTemplate(iniStr) ; continue; }
        // veloVariants
        if (tok == "VELOVARIANTS" || tok == "VELO_VARIANTS") {_veloVars = parseVariants(iniStr); continue; }
        // veloLimits
        if (tok == "VELOLIMITS" || tok == "VELO_LIMITS") {
          _veloCurve = VC_CUSTOM;
          parseLimits(iniStr); continue; 
        }
        break;
      case S_NOTE:
      case S_RANGE:
        // name = C2
        if (tok == "NAME") {range.first = midiNoteByName(iniStr); range.last = range.first; continue;}
        // or
        // first = C2
        if (tok == "FIRST") {range.first = midiNoteByName(iniStr); continue;}
        // last = F3
        if (tok == "LAST") {range.last = midiNoteByName(iniStr); continue;}
        // instr = Tom4
        if (tok == "INSTR") {range.instr = iniStr; continue;}
        // noteoff = 0
        if (tok == "NOTEOFF" || tok == "NOTE_OFF") {range.noteoff = parseBoolValue(iniStr); continue;}
        // speed = 1.15
        if (tok == "SPEED") {range.speed = parseFloatValue(iniStr); continue;}
        // limit_same_note = 1 // sometimes it makes sense to limit different instruments or notes differently (percussive sets, sound effects)
        // affects _keyboard[]
        if (tok == "LIMIT_SAME_NOTES" || tok == "LIMIT_SAME_NOTE" || tok == "LIMITSAMENOTE" || tok == "LIMITSAMENOTES") {
           range.limit_same = min(MAX_POLYPHONY, parseIntValue(iniStr));
           if (range.limit_same == 0 ) range.limit_same = MAX_POLYPHONY;
           continue;
        }
        // attack_time = 12
        if (tok == "ATTACKTIME" || tok == "ATTACK_TIME") {range.attack_time = parseFloatValue(iniStr); continue;}
        // decayTime = 0.05
        if (tok == "DECAYTIME" || tok == "DECAY_TIME") {range.decay_time = parseFloatValue(iniStr); continue;}
        // releaseTime = 12.0
        if (tok == "RELEASETIME" || tok == "RELEASE_TIME") {range.release_time = parseFloatValue(iniStr); continue;}
        // sustainLevel = 1.0
        if (tok == "SUSTAINLEVEL" || tok == "SUSTAIN_LEVEL") {range.sustain_level = parseFloatValue(iniStr); continue;}
        
        // loop = true // simple all-file infinite forward loop 
        if (tok == "LOOP" || tok == "AUTOREPEAT" || tok == "REPEAT" || tok == "CYCLE" ) {range.loop = parseBoolValue(iniStr); continue;}
        break;
      case S_GROUP:
        // notes = c1,c2,c3
        break;
      case S_NONE:
      case S_SAMPLESET:
      default:
        // title = Coda88 Piano 
        if (tok == "TITLE") {_title = iniStr; continue;}
        // type=melodic 
        if (tok == "TYPE") {
          _type = parseInstrType(iniStr);
          for (int i=0; i<128; ++i) {
              _keyboard[i].noteoff = (_type!=SMP_PERCUSSIVE);
          }
          continue;
        }
        // normalized=true 
        if (tok == "NORMALIZED") {_normalized = parseBoolValue(iniStr); continue;}
        // amp = 1.1 // 10% "louder"
        if (tok == "AMP" || tok == "AMPLIFY") {_amp = parseFloatValue(iniStr); continue;}
        // limit_same_note = 0 // 0 = unlimited same note triggering, 1 and more limits simultanous sounding of same notes
        // affects _keyboard[]
        if (tok == "LIMIT_SAME_NOTES" || tok == "LIMIT_SAME_NOTE" || tok == "LIMITSAMENOTE" || tok == "LIMITSAMENOTES") {
          _limitSameNotes = min(MAX_POLYPHONY, parseIntValue(iniStr));
          if (_limitSameNotes == 0) _limitSameNotes = MAX_POLYPHONY;
          for (int i=0; i<128; ++i) {
              _keyboard[i].limit_same = _limitSameNotes;
          }
          continue;
        }
        // max_voices = 12 // sometimes you may want to overwrite polyphony setting, but it will be no more than #defime MAX_POLYPHONY
        if (tok == "MAX_VOICES" || tok == "MAX_POLYPHONY" || tok == "MAXPOLYPHONY" || tok == "MAXVOICES" || tok == "POLYPHONY") {_maxVoices = min(MAX_POLYPHONY, parseIntValue(iniStr)); continue;}
        
        // attackTime = 0.0
        if (tok == "ATTACKTIME" || tok == "ATTACK_TIME") {_attackTime = parseFloatValue(iniStr); setAttackTime(_attackTime); continue;}
        // decayTime = 0.05
        if (tok == "DECAYTIME" || tok == "DECAY_TIME") {_decayTime = parseFloatValue(iniStr); setDecayTime(_decayTime); continue;}
        // releaseTime = 12.0
        if (tok == "RELEASETIME" || tok == "RELEASE_TIME") {_releaseTime = parseFloatValue(iniStr); setReleaseTime(_releaseTime); continue;}
        // sustainLevel = 1.0
        if (tok == "SUSTAINLEVEL" || tok == "SUSTAIN_LEVEL") {_sustainLevel = parseFloatValue(iniStr); setSustainLevel(_sustainLevel); continue;}
        break;
    }    
  }
  // save parsed section
  if (section==S_NOTE || section==S_RANGE) applyRange(range);
  Reader.close();
  DEBUG("SAMPLER: INI: PARSING COMPLETE");
  //delay(1000);
}

eSection_t SamplerEngine::parseSection(str256_t& val) {
  // S_NONE, S_SAMPLESET, S_FILENAME, S_ENVELOPE, S_NOTE, S_RANGE, S_GROUP };
  //DEB(val.c_str());
  int len = val.length();
  val.remove(len-1);  // remove closing "]" 
  val.remove(0,1);    // remove opening "["
  val.trim();         // trim the inner string
  if (val == "SAMPLESET") return S_SAMPLESET;
  if (val == "FILENAME")  return S_FILENAME;
  if (val == "NOTE")      return S_NOTE;
  if (val == "RANGE")     return S_RANGE;
  if (val == "GROUP")     return S_GROUP;
  return S_NONE;
}

bool SamplerEngine::parseBoolValue(str256_t& val) {
  val.trim();
  val.toUpperCase();
  bool var = false;
  if (val=="TRUE" || val=="YES" || val=="Y" || val=="1") var = true;
  if (val=="FALSE" || val=="NO" || val=="NONE" || val=="N" || val=="0") var = false;
  return var;
}


float SamplerEngine::parseFloatValue( str256_t& val) {
  val.trim();
  val.toUpperCase();
  float f = val.toFloat();
  DEBF("INI: float %f\r\n", f);
  return f;
}


int SamplerEngine::parseIntValue( str256_t& val) {
  val.trim();
  val.toUpperCase();
  int i = val.toInt();
  DEBF("INI: int %d\r\n", i);
  return i;
}


variants_t SamplerEngine::parseVariants( str256_t& val) {
  DEBUG("INI: Parsing variants");
  variants_t vars;
  vars.clear();
  val.trim();
  val.toUpperCase();
  int j = 0;
  int k = 0;
  while (true) {
    str20_t s1;
    k = val.indexOf("," , j);
    if (k == -1 || k < j) k = val.length();
    s1 = val.substring(j, k);
    s1.trim();
    vars.push_back(s1);
    DEBF("Adding %s\r\n" , s1.c_str());
    if (k == val.length()) break;
    j = k + 1;
  }
  return vars;
}

void SamplerEngine::parseLimits( str256_t& val) {
  DEBUG("INI: Parsing velo limits");
  variants_t vars;
  vars.clear();
  val.trim();
  int j = 0;
  int k = 0;
  int lim1 = 0;
  int lim2 = 0;
  int layer = 0;
  while (true) {
    str20_t s1;
    k = val.indexOf("," , j);
    if (k == -1 || k < j) k = val.length();
    s1 = val.substring(j, k);
    s1.trim();
    lim2 = s1.toInt();
    for (int i = lim1; i <= lim2; i++) { _veloMap[i] = layer; }
    DEBF("Adding %s\r\n" , s1.c_str());
    layer++;
    lim1 = lim2 + 1;
    if (k == val.length()) break;
    if (lim1 > 127) break;
    j = k + 1;
  }
  _veloCurve = VC_CUSTOM;
}

eInstr_t SamplerEngine::parseInstrType(str256_t& val) {
  val.trim();
  val.toUpperCase(); 
  if (val == "MELODIC") return SMP_MELODIC;
  if (val == "PERCUSSIVE") return SMP_PERCUSSIVE;
  return SMP_MELODIC; // by default
}


void SamplerEngine::applyRange(ini_range_t& range) {
  _ranges.push_back(range);
  for (int i=range.first; i<=range.last; i++) {
    _keyboard[i].noteoff        = range.noteoff;
    _keyboard[i].tuning         = range.speed;
    _keyboard[i].limit_same     = range.limit_same;
    _keyboard[i].attack_time    = range.attack_time;
    _keyboard[i].decay_time     = range.decay_time;
    _keyboard[i].sustain_level  = range.sustain_level;
    _keyboard[i].release_time   = range.release_time;
  }
  DEBF("INI: adding range for %s\r\n", range.instr.c_str());
}


bool SamplerEngine::parseFilenameTemplate(str256_t& line) {
  DEBF("Parse filename template:[[[%s]]]\r\n", line.c_str());
  int a, b, i;
  str256_t str;
  template_item_t item;
  i = 0;
  while (true) {
    a = line.indexOf("<",i);
    b = line.indexOf(">",i);
    if (a < 0  ||  b < 0) break;
    if (b < a) return false;
    if (a > i && b > a) {  // at the start of separator
      str = line.substring(i, a );
      DEBF ("Parsing filename template: String separator: [%s]\r\n", str.c_str());
      item.item_type = P_SEPARATOR;
      item.item_str = str;
      _template.push_back(item);
      i = a;
    }
    if (a == i) { // at the start of token
      str = line.substring(a + 1, b );
      DEBF ("Parsing filename template: Token: [%s]\r\n", str.c_str());
      str = str.substring(0, 4);
      if (str ==  "NUMB" ) {
          item.item_type = P_NUMBER;
          item.item_str = "";
          _template.push_back(item);
      }
      else if (str == "NAME" ) {
          item.item_type = P_NAME;
          item.item_str = "";
          _template.push_back(item);
      }
      else if (str == "MIDI" ) {
          item.item_type = P_MIDINOTE;
          item.item_str = "";
          _template.push_back(item);
      }
      else if (str == "OCTA" ) {
          item.item_type = P_OCTAVE;
          item.item_str = "";
          _template.push_back(item);
      }
      else if (str == "VELO" ) {
          item.item_type = P_VELO;
          item.item_str = "";
          _template.push_back(item);    
      }
      else if (str == "INST" ) {
          item.item_type = P_INSTRUMENT;
          item.item_str = "";
          _template.push_back(item);    
      }
    }
    i = b + 1;
    if (i >= line.length()) return true;
  }
  return true;
}

void SamplerEngine::processNameParser(entry_t* entry) {
  int pos = 0;
  int i = 0;
  int range_i = 0;
  int len = 0;
  int oct = -2;
  int velo = 0;
  int note_num = -1;
  int midi_note_num = -1;
  std::vector<int> rng_i; // affected indices
  int match_weight = 0;
  str20_t instr = "";
  str20_t s;
  sample_t smp;
  fname_t fname = entry->name;
  fname.toUpperCase();
  if (fname.endsWith(".WAV")) {
    //DEBF("Parsing name: <%s>\r\n", fname.c_str());
  } else {
    DEBF("Skipping name: <%s>\r\n", fname.c_str());
    return;
  }
  for (auto tpl: _template) {
    switch ((int)tpl.item_type) {
      case P_NAME: // note name
        match_weight = 0;
        for (int j = 0; j < 2 ; j++) {
          for (int k = 0 ; k < 12; k++) {
            str8_t a = notes[j][k];
            len = a.length();
            str8_t b = fname.substring(pos, pos + len);
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
      //  DEBF("Note: %s\r\n", notes[0][note_num].c_str());
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
      case P_MIDINOTE:
        s = "";
        while (true) {
          if (fname.charAt(pos) >='0' && fname.charAt(pos) <='9' ) {
            s += fname.charAt(pos);
            pos++;
          } else {
            break;
          }
        }
        //DEBF("Midi Note Number: %s\r\n", s.c_str());
        if (s>"") midi_note_num = s.toInt();
        break;
      case P_INSTRUMENT: {
       // DEBUG("Filename: Parsing P_INSTRUMENT");
          i = 0;
          match_weight = 0;
          for (auto rng: _ranges) { // first pass: find max match_weight
            len = rng.instr.length();
            instr = fname.substring(pos, pos + len);
            // DEBF("INI: TEST instr: [%s] \r\n", instr.c_str());
            if (len>match_weight && instr.equalsIgnoreCase(rng.instr)) {
              match_weight = len;
            }
          }
          i = 0;
          rng_i.clear();
          for (auto rng: _ranges) { // second pass: we can have more than 1 match
            len = rng.instr.length();
            if (len == match_weight) {
              instr = fname.substring(pos, pos + len);
              if (instr.equalsIgnoreCase(rng.instr)) {
                // range matches by instrument
                rng_i.push_back(i);
                 DEBF("INI: Matched instr: %s, %d \r\n", instr.c_str(), i);
              }
            }
            i++;
          }
          if (match_weight>0) {
            pos += match_weight;
          } else {
            DEBUG("INI: Couldn't match instrument name");    
          }
        }
      case P_OCTAVE:
        s = fname.substring(pos, pos+1);
        oct = s.toInt();
//        DEBF("Octave: %d\r\n" , oct);
        pos++;
        break;
      case P_VELO:
      // DEBUG("Filename: Parsing P_VELO");
        i = 0;
        match_weight = 0;
        for(auto var: _veloVars) {
          len = var.length();
          s = fname.substring(pos, pos + len);
          if (len>match_weight && s.equalsIgnoreCase(var)) {
            match_weight = len;
            velo = i;
          }
          i++;
        }
        // DEBF("Velocity: %d\r\n", velo);
        pos += match_weight;
        break;
      case P_SEPARATOR:
        s = tpl.item_str;
        len = s.length();
        // DEBF("Skipping separator %s\r\n", s.c_str());
        pos += len;
        break;
      default:
        //impossible:
      break;
    }
  }
// if we have note name in filename
  if (note_num>=0 && oct>=-1) {
    midi_note_num = note_num + (oct+1)*12; // midi note number
  }  
  if (midi_note_num>=0) {
    if ( velo >= 0 ) {
      sample_t smp;
      smp.orig_velo_layer = velo;
      smp.sectors = entry->sectors;
      smp.size = entry->size;
      smp.native_freq = true;
      // smp.name = (entry->name);
      parseWavHeader(entry, smp);
      _sampleMap[midi_note_num][velo] = smp;
    }
  }

// if we have [ranges] or [notes] in ini
  if ( !rng_i.empty() ) {
    if (velo < 0) velo = 0;
    for (auto ix: rng_i) {
      for (int midi_note = _ranges[ix].first; midi_note<=_ranges[ix].last; midi_note++) {
        sample_t smp;
        smp.orig_velo_layer = velo;
        smp.sectors = entry->sectors;
        smp.size = entry->size;
        smp.native_freq = true;
        // smp.name = (entry->name);
        parseWavHeader(entry, smp);
        _sampleMap[midi_note][velo] = smp;
      }
    }
  }
}


void SamplerEngine::parseWavHeader(entry_t* wav_entry, sample_t& smp){
  char* buf = reinterpret_cast<char*>(_Card->readFirstSector(wav_entry)); // massive long headers won't pass here (
  wav_header_t* wav = reinterpret_cast<wav_header_t*>(buf);
  smp.sample_rate = wav->sampleRate;
  smp.bit_depth = wav->bitsPerSample;
  smp.channels = wav->numberOfChannels;
  smp.speed = (float)(wav->sampleRate) * DIV_SAMPLE_RATE;
  int res = -1;
  for (int i = 0 ; i < BYTES_PER_SECTOR-4; i++) {
    if (buf[i]=='d' && buf[i+1]=='a' && buf[i+2]=='t' && buf[i+3]=='a') {
      res = i;
      break;
    }
  }
  if (res >= 0 ) {
    smp.byte_offset = res + 8;
    smp.data_size = *(reinterpret_cast<uint32_t*>(&buf[res+4]));
  }
}



// fill gaps
void SamplerEngine::finalizeMapping() {
  int x, y, dx, dy, xc, yc, s;
  int max_v = 0;
  int n = 1 + 2 * MAX_DISTANCE_STRETCH;
  sample_t smp;
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
  switch((int)_type) {
    case SMP_PERCUSSIVE:
      for (int i = 0; i < 128; i++) {
        smp.bit_depth = 0;
        for (int j = 0; j < _veloLayers; j++) {
          if (_sampleMap[i][j].bit_depth > 0) {
            smp = _sampleMap[i][j];
          } else {
            if (smp.bit_depth > 0) {
              _sampleMap[i][j] = smp;
            }
          }
        }
        for (int j = _veloLayers-1; j >=0; j--) {
          if (_sampleMap[i][j].bit_depth > 0) {
            smp = _sampleMap[i][j];
          } else {
            if (smp.bit_depth > 0) {
              _sampleMap[i][j] = smp;
            }
          }
        }
      }
      break;
    case SMP_MELODIC:
    default:
      // second pass
      for (int i = 0; i < _veloLayers; i++) {
        // going up
        for (int j = 0; j < 128; j++) {
          if (_sampleMap[j][i].speed == 0.0f) {
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
                if (_sampleMap[xc][yc].speed > 0.0f ) {
                  _sampleMap[j][i] = _sampleMap[xc][yc];
                  _sampleMap[j][i].speed = _sampleMap[xc][yc].speed * _keyboard[j].freq / _keyboard[x].freq;
                  _sampleMap[j][i].native_freq = false;
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
                if (_sampleMap[xc][yc].native_freq) {
                  _sampleMap[j][i] = _sampleMap[xc][yc];
                  _sampleMap[j][i].speed = _sampleMap[xc][yc].speed * _keyboard[j].freq / _keyboard[x].freq;
                  _sampleMap[j][i].native_freq = false;
                  break;  
                }
              } 
            }
          }
        }
      }
      // VC_LINEAR, VC_SOFT1, VC_SOFT2, VC_SOFT3, VC_HARD1, VC_HARD2, VC_HARD3, VC_CONST, VC_NUMBER
  }
  // propagate params to individual samples
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j < 128; j++ ) {
      _sampleMap[j][i].speed         *= _keyboard[j].tuning;
      _sampleMap[j][i].amp           *= _amp;
      _sampleMap[j][i].attack_time    = _keyboard[j].attack_time;
      _sampleMap[j][i].decay_time     = _keyboard[j].decay_time;
      _sampleMap[j][i].sustain_level  = _keyboard[j].sustain_level;
      _sampleMap[j][i].release_time   = _keyboard[j].release_time;
    }
  }
}


uint8_t SamplerEngine::midiNoteByName(str8_t noteName) {
  int match_weight = 0;
  int len, note_num = -1;
  int oct = -3;
  for (int j = 0; j < 2 ; j++) {
    for (int k = 0 ; k < 12; k++) {
      str8_t a = notes[j][k];
      len = a.length();
      str8_t b = noteName.substring(0,len);
      if (a.equalsIgnoreCase(b) && len > match_weight) {
        match_weight = len;
        note_num = k;
      }
    }
  }
  str8_t b = noteName.substring(match_weight, match_weight+2);
  DEBF("INI: midiNoteByName string: [%s] note: [%d] oct: [%s] \r\n", noteName.c_str(), note_num, b.c_str());
  oct = b.toInt();
  if (oct>-2) {
    note_num+=(oct+1)*12;
  }
  DEBUG(note_num);
  return note_num;
}


void SamplerEngine::printMapping() {
  for (int i = 0; i < 128; i++) {
    DEBF("%s%d\t", notes[1][i%12].c_str(), i/12 -1 );
  }
  DEBUG(",");
  for (int i = 0; i < _veloLayers; i++) {
    for (int j = 0 ; j<128; j++ ) {
      if (!_sampleMap[j][i].sectors.empty()) {
        DEBF("%d %3.2f\t",_sampleMap[j][i].native_freq, _sampleMap[j][i].speed );
      //  DEBF("%s\t", _sampleMap[j][i].name.c_str() );
      } else {        
        DEBF( "%d\t", 0 );
      }
    }
    DEBUG(".");
  }
}
