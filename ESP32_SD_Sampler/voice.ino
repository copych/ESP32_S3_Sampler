#include "voice.h"

bool Voice::allocateBuffers() {
  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
  _buffer0 = (uint8_t*)heap_caps_malloc( BUF_SIZE_BYTES + BUF_EXTRA_BYTES , MALLOC_CAP_INTERNAL);
  _buffer1 = (uint8_t*)heap_caps_malloc( BUF_SIZE_BYTES + BUF_EXTRA_BYTES , MALLOC_CAP_INTERNAL);
  if( _buffer0 == NULL || _buffer1 == NULL){
    DEBUG("No more RAM for sampler buffer!");
    return false;
  } else {
    DEBF("%d Bytes RAM allocated for sampler buffers, &_buffer0=%#010x\r\n", BUF_NUMBER * ( BUF_SIZE_BYTES + BUF_EXTRA_BYTES ) , _buffer0);
  }
  return true;
}


void Voice::init(SDMMC_FAT32* Card, bool* sustain, bool* normalized){
  _Card = Card;
  _sustain = sustain;
  _normalized = normalized;
  if (!allocateBuffers()) {
    DEBUG("VOICE: INIT: NOT ENOUGH MEMORY");
    delay(100);
    while(1){;}
  }
  AmpEnv.init(SAMPLE_RATE);
  AmpEnv.end(Adsr::END_NOW);
  _active   = false;
  _midiNote = 255;
  _queued   = false;
  _pressed  = false;
  _eof      = true;
}


// If the voice is free, it sets the new sample to play
// If the voice is active, it queues a new sample and calls ADSR.end(END_FAST) which fades really fast (8 samples or so)
void Voice::start(const sample_t smpFile, uint8_t midiNote, uint8_t midiVelo) { // executed in Control Task (Core1)
  //taskENTER_CRITICAL(&noteonMux);
 // DEBF ("VOICE: START: active = %d, queued = %d\r\n", _active, _queued);
 // if (!_active || _queued) {
 //   _queued                 = false;
    _sampleFile             = smpFile;
    _bytesToRead            = smpFile.size;
    _bytesToPlay            = smpFile.byte_offset + smpFile.data_size;
    _amplitude              = 0.0f;
    _bytesPlayed            = 0;
    _coarseBytesPlayed      = 0;
    _fullSampleBytes        = smpFile.channels * smpFile.bit_depth / 8;
    _speed                  = smpFile.speed;
    _bufSizeSmp             = BUF_SIZE_BYTES / _fullSampleBytes;
    _bufEmpty[0]            = true;
    _bufEmpty[1]            = true;
    _bufPosSmp[0]           = _bufSizeSmp;
    _bufPosSmp[1]           = _bufSizeSmp;
    _eof                    = false; 
    _bufPlayed              = 0;
    _fillBuffer             = _buffer0;
    _playBuffer             = _buffer1;
    _idToFill               = 0;
    _idToPlay               = 1;
    _curChain               = 0;
    _loop                   = (smpFile.loop_mode > 0);
    if (_loop) {
      if (smpFile.loop_first_smp >=0 ) {
        _loopFirstSmp = smpFile.loop_first_smp;
      } else {
        _loopFirstSmp = 0;
      }
      if (smpFile.loop_last_smp >=0 ) {
        _loopLastSmp = smpFile.loop_last_smp;
      } else {
        _loopLastSmp = _bytesToPlay / _fullSampleBytes - 1;
      }
      _loopFirstSector = (smpFile.byte_offset + _fullSampleBytes * _loopFirstSmp ) / BYTES_PER_SECTOR;
      _loopLastSector  = (smpFile.byte_offset + _fullSampleBytes * _loopLastSmp ) / BYTES_PER_SECTOR;
      
    }
    _pL1                    = start_byte[_fullSampleBytes / smpFile.channels];
    _pL2                    = _pL1 + _fullSampleBytes;
    _pR1                    = _pL1 + ( _fullSampleBytes / smpFile.channels );
    _pR2                    = _pR1 + _fullSampleBytes;
    if (smpFile.size == 0) {
      _divFileSize          = 0.001f;
    } else {
      _divFileSize          = 1.0f/((float)smpFile.data_size);
    }
    _lastSectorRead         = smpFile.sectors[_curChain].first - 1; 
    _midiNote = midiNote;
    _midiVelo = midiVelo; 
    if (*_normalized) {
      _amp = (float)_midiVelo * MIDI_NORM * 0.000033f;
    } else {
      _amp =   0.000033f;
    }
    
    if (midiVelo > 0) {
      _divVelo = 1.0f/((float)midiVelo);
    } else {
      _divVelo = 256.0f;
    }
    _killScoreCoef = (float)_fullSampleBytes * (float)_divFileSize * (float)_divVelo;
    
    _killScoreCoef =  (float)_divFileSize;
    _hungerCoef = (float)_fullSampleBytes * (float)_speed;
     DEBF("VOICE: start note %d velo %d\r\n", midiNote, midiVelo);
    AmpEnv.retrigger(Adsr::END_NOW);
    _active = true;
    _dying = false;
    _pressed = true;
    /*
  } else  {
    DEBF("VOICE: START: QUEUE: note %d velo %d \r\n", midiNote, midiVelo);
    _queued = true;
    _nextFile = smpFile;
    _nextNote = midiNote;
    _nextVelo = midiVelo;
    AmpEnv.end(Adsr::END_FAST);
  }
  */
  //taskEXIT_CRITICAL(&noteonMux);
}


void Voice::end(Adsr::eEnd_t end_type){ // most likely being executed in Control Task (Core1)
  switch ((int)end_type) {
    case Adsr::END_NOW:{
      AmpEnv.end(Adsr::END_NOW);
      DEBF("VOICE: END: NOW %d\r\n", _midiNote); 
      _active = false;
      _midiNote = 255;
      _amplitude = 0.0;
      break;
    }
    case Adsr::END_FAST:{
      _dying = true;
      AmpEnv.end(Adsr::END_FAST);
      DEBF("VOICE: END: FAST %d\r\n", _midiNote);
      break;
    }
    case Adsr::END_REGULAR:
    default:{
      if (!_pressed && !(*_sustain)) {
        DEBF("VOICE: END: REGULAR %d\r\n", _midiNote); 
        AmpEnv.end(Adsr::END_REGULAR);
      }
    }
  }
}

void Voice::getSample(float& sampleL, float& sampleR) {
  static float env;
  static int pos;
  float l1, l2, r1, r2;
  sampleL = sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  if (_active) {
    env =  (float)AmpEnv.process() * (float)_amp ;
    // env = _amp;
    if (AmpEnv.isIdle()) {      
  //      if (_queued) {
  //        DEBF("VOICE: QUEUE START note %d velo %d\r\n", _nextNote, _nextVelo);
  //        start(_nextFile, _nextNote, _nextVelo);
  //      } else {
          _active = false;
          _dying = false;
          _midiNote = 255;
  //     }
       _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;

    } else {
      pos = (uint32_t)_playBufOffset + (uint32_t)_fullSampleBytes * (uint32_t)_bufPosSmp[_idToPlay ];  // pos in a byte buffer
      
      l1 = *( reinterpret_cast<volatile int16_t*>( &_playBuffer[ pos + _pL1 ] ) );
      l2 = *( reinterpret_cast<volatile int16_t*>( &_playBuffer[ pos + _pL2 ] ) );
      sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
      
      if (_sampleFile.channels == 2){
        r1 = *( reinterpret_cast<volatile int16_t*>( &_playBuffer[ pos + _pR1 ] ) );
        r2 = *( reinterpret_cast<volatile int16_t*>( &_playBuffer[ pos + _pR2 ] ) );
        sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;
      } else {        
        sampleR = sampleL;
      }
      
      _bufPosSmpF += (float)_speed ; // * _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;

      _bytesPlayed = _coarseBytesPlayed + (uint32_t)_bufPosSmp[_idToPlay] * (uint32_t)_fullSampleBytes;
      
      /*
      if ( _bytesPlayed % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)fabs(sampleL) + (float)fabs(sampleR);
      } 
      */
      if ( _bytesPlayed >= _bytesToPlay ) {
        end(Adsr::END_NOW);
        /*
        if (_queued) {
          start(_nextFile, _nextNote, _nextVelo);
        } 
         DEBF("VOICE: DATA END: bytes played = %d , bytes to play = %d , pos in buffer = %d \r\n", _bytesPlayed , _bytesToPlay, _bufPosSmp[_idToPlay]);
        */
      }
      if (_bufPosSmp[_idToPlay ]  >= _samplesInPlayBuf ) {
        toggleBuf();        
      }
    }
  }
}

void  Voice::feed() { // executed in Control Task (Core1)
  if (_bufEmpty[_idToFill] && !_eof) {
    if (_loop) {
      int bytes_till_loop_end = _loopLastSmp * _fullSampleBytes - _bytesPlayed;
      float fill_coef =  (float)bytes_till_loop_end / (float)BUF_SIZE_BYTES ;
      if (fill_coef >= 1.0f && fill_coef < 1.42f) {
        // change buffer size to allow looping wav data to be loaded from SD (not just a few bytes to play before rewinding to the loop start)
        // divide the remaining bytes appx by half
        
      } 
    }
    int sectorsToRead = READ_BUF_SECTORS;
    int sectorsAvailable;
    volatile uint8_t* bufAddr =  _fillBuffer;
   // DEBF("fill buf addr %d\r\n", bufAddr);
    while (sectorsToRead > 0) {
      sectorsAvailable = min(_sampleFile.sectors[_curChain].last - _lastSectorRead, (uint32_t) sectorsToRead) ;
      if (sectorsAvailable > 0) { // we have some sectors in the current chain to read
        // DEBF("block available = %d Pointer = %010x\r\n", sectorsAvailable, bufAddr);
        _Card->read_block((uint8_t*)bufAddr, _lastSectorRead+1, sectorsAvailable);
        _lastSectorRead += sectorsAvailable;
        sectorsToRead -= sectorsAvailable;
        _bytesToRead -= sectorsAvailable * BYTES_PER_SECTOR;
        bufAddr += sectorsAvailable * BYTES_PER_SECTOR;
      } else { // we've done with the current chain
        if (_curChain + 1 < _sampleFile.sectors.size()) { // we still got some sectors to read
          _curChain++;
          _lastSectorRead = _sampleFile.sectors[_curChain].first - 1;
        } else { // this was the last chain of sectors
          _eof = true;
          break;
        }
      }
      // copy first bytes to playBuffer's extra zone
      memcpy((void*)(_playBuffer + BUF_SIZE_BYTES), (const void*)(_fillBuffer ), BUF_EXTRA_BYTES);
    }
    if (_bufEmpty[0] && _bufEmpty[1]) { // init state: bufToFill = 0, bufToPlay = 1
      _bufEmpty[_idToFill]    = false; // bufToFill is now filled with the first sectors of sample file
      _idToFill               = 1;
      _idToPlay               = 0;
      _playBuffer             = _buffer0;
      _fillBuffer             = _buffer1;
      _bufPosSmp[_idToFill]   = _bufSizeSmp;
      _bufPosSmp[_idToPlay]   = 0;  
      _bufPosSmpF             = _bufPosSmp[_idToPlay];
      _fillBufOffset = _playBufOffset = _sampleFile.byte_offset ;
      _samplesInFillBuf = _samplesInPlayBuf = (BUF_SIZE_BYTES - _playBufOffset) / _fullSampleBytes ;
    } else {
      _bufEmpty[_idToFill]    = false; // bufToFill is now filled with the continous sectors of sample file
      _bufPosSmp[_idToFill]   = 0;
      _fillBufOffset = ( _fullSampleBytes - ( (BUF_SIZE_BYTES - _playBufOffset) % _fullSampleBytes )) % _fullSampleBytes ;
      _samplesInFillBuf = ((int)BUF_SIZE_BYTES - _fillBufOffset ) / (int)_fullSampleBytes ;
    }
  }
}


inline void Voice::toggleBuf(){  
  // DEBF("VOICE: TOGGLE: pos: %d, inBuf: %d, _offset: %d, bytesPlayed: %d \r\n", _bufPosSmp[_idToPlay ], _samplesInPlayBuf, _offset, _bytesPlayed );
  _bufEmpty[_idToPlay ] = true;  
  _playBufOffset = _fillBufOffset;
  _bufPosSmpF -= (float)_samplesInPlayBuf;
  //_coarseBytesPlayed += _samplesInPlayBuf;
  _coarseBytesPlayed = _bytesPlayed;
  _bufPlayed++;
  
  _samplesInPlayBuf = _samplesInFillBuf;
  switch(_idToPlay ) { 
    case 0:
      _playBuffer  = _buffer1;
      _fillBuffer  = _buffer0;
      _idToPlay  = 1;
      _idToFill  = 0;
      break;
    case 1:
      _playBuffer  = _buffer0;
      _fillBuffer  = _buffer1;
      _idToPlay  = 0;
      _idToFill  = 1;
  }
  //  DEBF("&playBuf=%#010x &fillBuf=%#010x\r\n", _buffer0, _fillBuffer);
}


uint32_t Voice::hunger() { // called by SamplerEngine::fillBuffer() in ControlTast, Core1
    if (!_active || _eof || _queued || _dying || !(_bufEmpty[0] || _bufEmpty[1])) return 0;
    return (/*(float)_speedModifier * */(float)_hungerCoef * ((float)_bufPosSmp[0] + (float)_bufPosSmp[1])); // the bigger the value, the sooner we empty the buffer
}


// linear interpolation of 2 neighbour values by float index (mantissa only used)
inline float Voice::interpolate(float& v1, float& v2, float index) {
  float res;
  int32_t i = (int32_t)index;
  float f = (float)index - i;
  res = (float)f * (float)(v2 - v1) + (float)v1;
  return res;
}


inline float Voice::getKillScore() {
  if (_dying || _queued) return 0.0f;
  return (float)_bytesPlayed * (float)_killScoreCoef ;
}
