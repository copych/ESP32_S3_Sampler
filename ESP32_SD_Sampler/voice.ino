#include "voice.h"

bool Voice::allocateBuffers() {
  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
  _buffer0 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES + BUF_EXTRA_BYTES , MALLOC_CAP_INTERNAL);
  _buffer1 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES + BUF_EXTRA_BYTES , MALLOC_CAP_INTERNAL);
  _fadeL = (float*)heap_caps_malloc( FADE_OUT_SAMPLES * sizeof(float) , MALLOC_CAP_INTERNAL);
  _fadeR = (float*)heap_caps_malloc( FADE_OUT_SAMPLES * sizeof(float) , MALLOC_CAP_INTERNAL);
  if( _buffer0 == NULL || _buffer1 == NULL){
    DEBUG("No more RAM for sampler buffer!");
    return false;
  } else {
    for (int i=0; i< FADE_OUT_SAMPLES; i++) {
      _fadeL[i] = _fadeR[i] = 0.0f;
    } 
    DEBF("%d Bytes RAM allocated for sampler buffers, &_buffer0=%#010x\r\n", BUF_NUMBER * ( BUF_SIZE_BYTES + BUF_EXTRA_BYTES ) + 2 * FADE_OUT_SAMPLES * sizeof(float), _buffer0);
  }
  return true;
}


void Voice::init(SDMMC_FAT32* Card, bool* sustain){
  _Card = Card;
  _sustain = sustain;
  if (!allocateBuffers()) {
    DEBUG("VOICE: INIT: NOT ENOUGH MEMORY");
    delay(100);
    while(1){;}
  }
  AmpEnv.init(SAMPLE_RATE);
  AmpEnv.end(Adsr::END_NOW);
  _active   = false;
  _pressed  = false;
  _eof      = true;
}


void Voice::start(const sample_t smpFile, uint8_t midiNote, uint8_t midiVelo){
  //taskENTER_CRITICAL(&noteonMux);
    _sampleFile             = smpFile;
    _bytesToRead            = smpFile.size;
    // _amplitude              = 0.0f;
    _smpSize                = smpFile.channels * smpFile.bit_depth / 8;
    _speed                  = smpFile.speed;
    _bufSizeSmp             = BUF_SIZE_BYTES / _smpSize;
    _bufEmpty[0]            = true;
    _bufEmpty[1]            = true;
    _bufPosSmp[0]           = _bufSizeSmp;
    _bufPosSmp[1]           = _bufSizeSmp;
    _eof                    = false; 
    _bufPlayed              = 0;
    _fillBuf16              = _buffer0;
    _playBuf16              = _buffer1;
    _fillBuf8               = (uint8_t*)_buffer0;
    _playBuf8               = (uint8_t*)_buffer1;
    _idToFill               = 0;
    _idToPlay               = 1;
    _curChain               = 0;
    switch (smpFile.bit_depth) {
      case 24:
        switch (smpFile.channels) {
          case 1:
            _bufCoef = 1;
            getSampleVariants = &Voice::getSample24m;
            break;
          case 2:
          default:
            _bufCoef = 1;
            getSampleVariants = &Voice::getSample24s;
        }
        break;
      case 16:
      default:
        switch (smpFile.channels) {
          case 1:
            _bufCoef = 2;
            getSampleVariants = &Voice::getSample16m;
            break;
          case 2:
          default:
            _bufCoef = 2;
            getSampleVariants = &Voice::getSample16s;
        }
    }
    if (smpFile.size == 0) {
      _divFileSize          = 0.001f;
    } else {
      _divFileSize          = one_div((float)smpFile.size);
    }
    _lastSectorRead         = smpFile.sectors[_curChain].first - 1; 
    _midiNote = midiNote;
    _midiVelo = midiVelo; 
    _amp = (float)_midiVelo * MIDI_NORM * 0.000033f;
    /*
    if (midiVelo > 0) {
      _divVelo = one_div((float)midiVelo);
    } else {
      _divVelo = 256.0f;
    }
    _feedScoreCoef = (float)_smpSize * (float)_divFileSize * (float)_divVelo;
    */ 
    _feedScoreCoef = (float)_smpSize * (float)_divFileSize;
    _hungerCoef = (float)_smpSize * (float)_speed;
    // DEBF("VOICE: start note %d velo %d\r\n", midiNote, midiVelo);
    AmpEnv.retrigger(Adsr::END_NOW);
    _active = true;
    _pressed = true;
  
  //taskEXIT_CRITICAL(&noteonMux);
}


void Voice::end(Adsr::eEnd_t end_type){
  switch ((int)end_type) {
    case Adsr::END_NOW:{
      _active = false;
      // _amplitude = 0.0;
      AmpEnv.end(Adsr::END_NOW);
      break;
    }
    case Adsr::END_FAST:{
      AmpEnv.end(Adsr::END_FAST);
      fadeOut();
      _active = false;
      _fadePoint = 0;
      break;
    }
    case Adsr::END_REGULAR:
    default:{
      if (!_pressed && !(*_sustain)) {
        //  DEBF("VOICE: soft off note %d\r\n", _midiNote); 
        AmpEnv.end(Adsr::END_REGULAR);
      }
    }
  }
}


void Voice::fadeOut() {
  for (int i = 0; i < FADE_OUT_SAMPLES; i++) {
    getSample((float&)_fadeL[i], (float&)_fadeR[i]); 
  }
}


void Voice::getFadeSample(volatile float& sL, volatile float& sR){
  if (_fadePoint < FADE_OUT_SAMPLES) { 
    sL = _fadeL[_fadePoint];
    sR = _fadeR[_fadePoint];
    _fadePoint++;
    //DEBF("%d: %7.7f \t %7.7f \r\n", _fadePoint, sL, sR);
  } else {
    sL = 0.0f;
    sR = 0.0f;
  }
}


void Voice::getSample16m(float& sampleL, float& sampleR) {
  static int pos;
  static float env;
  static uint32_t bytes_played;
  float l1, l2, r1, r2;
  sampleL = sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  if (_active) {
    env =  (float)AmpEnv.process() * (float)_amp ;
    //env = _amp;
    if (AmpEnv.isIdle()) {
      _active = false;
      // _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;
    } else {   
      pos = _bufPosSmp[_idToPlay ] + _offset; 

      l1 = _playBuf16[ pos ];
      l2 = _playBuf16[ pos + 1 ];
      
      sampleL = sampleR = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
 
      bytes_played = ((uint32_t)_bufPlayed * (uint32_t)_bufSizeSmp + (uint32_t)_bufPosSmp[_idToPlay]) * (uint32_t)_smpSize;
      
      /*
      if ( bytes_played % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)fabs(sampleL) + (float)fabs(sampleR);
      }
      */
      
      if ( bytes_played >= _sampleFile.size ) {
        end(Adsr::END_NOW);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
      //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ;//* _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ]  >= _samplesInBuf ) {
      //   DEBF("VOICE: TOGGLE: pos: %d, inBuf: %d, _offset: %d, bytes_played: %d \r\n", _bufPosSmp[_idToPlay ], _samplesInBuf, _offset, bytes_played );
        toggleBuf();
      }
    }
  }
  if (_fadePoint < FADE_OUT_SAMPLES) { 
    sampleL += _fadeL[_fadePoint];
    sampleR += _fadeR[_fadePoint];
    _fadePoint++;
    //DEBF("%d: %7.7f \t %7.7f \r\n", _fadePoint, sL, sR);
  }
}


void Voice::getSample16s(float& sampleL, float& sampleR) {
  static int pos;
  static float env;
  static uint32_t bytes_played;
  float l1, l2, r1, r2;
  sampleL = sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  if (_active) {
    env =  (float)AmpEnv.process() * (float)_amp ;
    //if (AmpEnv.getCurrentSegment() == Adsr::ADSR_SEG_RELEASE) DEBUG (env);
    //env = _amp;
    if (AmpEnv.isIdle()) {
      _active = false;
      // _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;
    } else {
      pos = 2 * _bufPosSmp[_idToPlay ] + _offset; 
      // DEBF("pos = %d\r\n", pos);
    
      l1 = _playBuf16[ pos ];
      r1 = _playBuf16[ pos + 1 ];
      l2 = _playBuf16[ pos + 2 ];
      r2 = _playBuf16[ pos + 3 ];
      
      sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
      sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;

      bytes_played = ((uint32_t)_bufPlayed * (uint32_t)_bufSizeSmp + (uint32_t)_bufPosSmp[_idToPlay]) * (uint32_t)_smpSize;
      
      /*
      if ( bytes_played % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)sampleL + (float)sampleR;
      } 
      */
      if ( bytes_played >= _sampleFile.size ) {
        end(Adsr::END_NOW);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
       //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ; // * _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ]  >= _samplesInBuf ) {
       //   DEBF("VOICE: TOGGLE: pos: %d, inBuf: %d, _offset: %d, bytes_played: %d \r\n", _bufPosSmp[_idToPlay ], _samplesInBuf, _offset, bytes_played );
        toggleBuf();        
      }
    }
  }
  if (_fadePoint < FADE_OUT_SAMPLES) { 
    sampleL += _fadeL[_fadePoint];
    sampleR += _fadeR[_fadePoint];
    _fadePoint++;
    //DEBF("%d: %7.7f \t %7.7f \r\n", _fadePoint, sL, sR);
  }
}


void Voice::getSample24m(float& sampleL, float& sampleR) {
  static int pos;
  static float env;
  static uint32_t bytes_played;
  float l1, l2, r1, r2;
  sampleL = sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  if (_active) {
    env =  (float)AmpEnv.process() * (float)_amp ;
    //env = _amp;
    if (AmpEnv.isIdle()) {
      _active = false;
      // _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;
    } else {
      // 24 bit mono
      pos = (uint32_t)_offset + (uint32_t)_smpSize * (uint32_t)_bufPosSmp[_idToPlay ];  // pos in a byte buffer
      // DEBF("pos = %d\r\n", pos);

      l1 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+1]) | static_cast<uint16_t>(_playBuf8[pos+2])<<8 );
      l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+4]) | static_cast<uint16_t>(_playBuf8[pos+5])<<8 );

      sampleL = sampleR = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;

      bytes_played = (uint32_t)_bufPlayed * BUF_SIZE_BYTES + (uint32_t)pos + (uint32_t)_smpSize;
      
      /*
      if ( bytes_played % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)sampleL + (float)sampleR;
      } 
      */
      if ( bytes_played >= _sampleFile.size ) {
        end(Adsr::END_NOW);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
   //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ;//* _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ]  >= _samplesInBuf ) {
     //   DEBF("VOICE: TOGGLE: pos: %d, inBuf: %d, _offset: %d, bytes_played: %d \r\n", _bufPosSmp[_idToPlay ], _samplesInBuf, _offset, bytes_played );
        toggleBuf();        
      }
    }
  }
  if (_fadePoint < FADE_OUT_SAMPLES) { 
    sampleL += _fadeL[_fadePoint];
    sampleR += _fadeR[_fadePoint];
    _fadePoint++;
    //DEBF("%d: %7.7f \t %7.7f \r\n", _fadePoint, sL, sR);
  }
}


void Voice::getSample24s(float& sampleL, float& sampleR) {
  static int pos;
  static float env;
  static uint32_t bytes_played;
  float l1, l2, r1, r2;
  sampleL = sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  if (_active) {
    env =  (float)AmpEnv.process() * (float)_amp ;
    //env = _amp;
    if (AmpEnv.isIdle()) {
      _active = false;
      // _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;
    } else {
      pos = (uint32_t)_offset + (uint32_t)_smpSize * (uint32_t)_bufPosSmp[_idToPlay ];  // pos in a byte buffer
      // DEBF("pos = %d\r\n", pos);

      l1 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+1]) | static_cast<uint16_t>(_playBuf8[pos+2])<<8 );
      r1 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+4]) | static_cast<uint16_t>(_playBuf8[pos+5])<<8 );
      l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+7]) | static_cast<uint16_t>(_playBuf8[pos+8])<<8 );
      r2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+10]) | static_cast<uint16_t>(_playBuf8[pos+11])<<8 );

      // if (fabs(r1-r2)>10000.0f) DEBF("pos: %d smpPos: %d fPos: %f _offset: %d \r\n", pos, _bufPosSmp[_idToPlay ], _bufPosSmpF, _offset);
      
      sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
      sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;
      
      bytes_played = (uint32_t)_bufPlayed * BUF_SIZE_BYTES + (uint32_t)pos + (uint32_t)_smpSize;
      
      /*
      if ( bytes_played % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)sampleL + (float)sampleR;
      } 
      */
      if ( bytes_played >= _sampleFile.size ) {
        end(Adsr::END_NOW);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
      //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ;//* _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ]  >= _samplesInBuf ) {
      //   DEBF("VOICE: TOGGLE: pos: %d, inBuf: %d, _offset: %d, bytes_played: %d \r\n", _bufPosSmp[_idToPlay ], _samplesInBuf, _offset, bytes_played );
        toggleBuf();        
      }
    }
  }
  if (_fadePoint < FADE_OUT_SAMPLES) { 
    sampleL += _fadeL[_fadePoint];
    sampleR += _fadeR[_fadePoint];
    _fadePoint++;
    //DEBF("%d: %7.7f \t %7.7f \r\n", _fadePoint, sL, sR);
  }
}


void  Voice::feed() {
  //volatile size_t t1,t2;
  //t1 = micros();
  if (_bufEmpty[_idToFill] && !_eof) {
    int sectorsToRead = READ_BUF_SECTORS;
    int sectorsAvailable;
    volatile int16_t* bufAddr =  _fillBuf16;
   // DEBF("fill buf addr %d\r\n", bufAddr);
    while (sectorsToRead > 0) {
      sectorsAvailable = min(_sampleFile.sectors[_curChain].last - _lastSectorRead, (uint32_t) sectorsToRead) ;
      if (sectorsAvailable > 0) { // we have some sectors in the current chain to read
        // DEBF("block available = %d Pointer = %010x\r\n", sectorsAvailable, bufAddr);
        _Card->read_block((int16_t*)bufAddr, _lastSectorRead+1, sectorsAvailable);
        _lastSectorRead += sectorsAvailable;
        sectorsToRead -= sectorsAvailable;
        _bytesToRead -= sectorsAvailable * BYTES_PER_SECTOR;
        bufAddr += sectorsAvailable * INTS_PER_SECTOR;
      } else { // we've done with the current chain
        if (_curChain + 1 < _sampleFile.sectors.size()) { // we still got some sectors to read
          _curChain++;
          _lastSectorRead = _sampleFile.sectors[_curChain].first - 1;
        } else { // this was the last chain of sectors
          _eof = true;
          break;
        }
      }
      // copy first bytes to extra zone
      memcpy((void*)(_playBuf8 + BUF_SIZE_BYTES), (const void*)(_fillBuf8 ), BUF_EXTRA_BYTES);
    }
    if (_bufEmpty[0] && _bufEmpty[1]) { // init state: bufToFill = 0, bufToPlay = 1
      _bufEmpty[_idToFill]    = false;
      _bufPosSmp[_idToFill]   = 0;
      _idToFill               = 1;
      _idToPlay               = 0;
      _playBuf16              = _buffer0;
      _fillBuf16              = _buffer1;
      _playBuf8               = (uint8_t*)_buffer0;
      _fillBuf8               = (uint8_t*)_buffer1;
      _bufPosSmp[_idToFill]   = _bufSizeSmp;
      _bufPosSmp[_idToPlay]   = 0; // skip wav header in buffer, position 0 will be used for interpolation
      _offset = _sampleFile.byte_offset / _bufCoef;
      _samplesInBuf = _bufSizeSmp - (_offset/_smpSize * _bufCoef);
      _bufPosSmpF =  _bufPosSmp[_idToPlay];
    } else {
      _bufEmpty[_idToFill]    = false;
      _bufPosSmp[_idToFill]   = 0;
      _samplesInBuf = _bufSizeSmp - (_offset/_smpSize * _bufCoef);
    }
  }
  //t2 = micros();
  //DEBF("Fill time %d\r\n", t2-t1);
}


inline void Voice::toggleBuf(){  
  _bufEmpty[_idToPlay ] = true;
  int remain = 0 ;
  switch (_sampleFile.bit_depth) {
    case 24:
      remain = (BUF_SIZE_BYTES - _offset) % _smpSize ;
      switch (_sampleFile.channels){
        case 1:             // 24 bit mono
          switch (remain) {
            case 0:
              _offset=0;
              break;
            case 1:
              _offset=2;
              break;
            case 2:
              _offset=1;
          }
          break;
        case 2:             // 24 bit stereo
          switch (remain) {
            case 0:
              _offset = 0;
              break;
            case 2:
              // we'll need 0, 2nd and 3rd bytes from a new buffer
              _offset = 4;
              break;
            case 4:
              // we will need  0 and 1st bytes from a new buffer
              _offset = 2;
          }
      }
      break;
    case 16:
    default:
      _offset = 0;
  }
  switch(_idToPlay ) { 
    case 0:
      _playBuf16 = _buffer1;
      _fillBuf16 = _buffer0;
      _playBuf8  = (uint8_t*)_buffer1;
      _fillBuf8  = (uint8_t*)_buffer0;
      _idToPlay  = 1;
      _idToFill  = 0;
      break;
    case 1:
    default:
      _playBuf16 = _buffer0;
      _fillBuf16 = _buffer1;
      _playBuf8  = (uint8_t*)_buffer0;
      _fillBuf8  = (uint8_t*)_buffer1;
      _idToPlay  = 0;
      _idToFill  = 1;
  }
  //  DEBF("&playBuf=%#010x &fillBuf=%#010x\r\n", _buffer0, _fillBuf16);

  _bufPosSmpF -= (float)_samplesInBuf;
  _bufPlayed++;
}

uint32_t Voice::hunger() {
    if (!_active || _eof || !(_bufEmpty[0] || _bufEmpty[1])) return 0;
    return (/*(float)_speedModifier * */(float)_hungerCoef * ((float)_bufPosSmp[0] + (float)_bufPosSmp[1]))   ; // the bigger the value, the sooner we empty the buffer
}


// linear interpolation of 2 neighbour values value by float index (mantissa only used)
inline float Voice::interpolate(float& v1, float& v2, float index) {
  float res;
  int32_t i = (int32_t)index;
  float f = (float)index - i;
  res = (float)f * (float)(v2 - v1) + (float)v1;
  return res;
}


inline float Voice::getFeedScore() {
  return (float)_bufPlayed * (float)_feedScoreCoef;
}
