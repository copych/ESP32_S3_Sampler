#include "voice.h"

bool Voice::allocateBuffers() {
  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
  _buffer0 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES , MALLOC_CAP_INTERNAL);
  _buffer1 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES , MALLOC_CAP_INTERNAL);
  if( _buffer0 == NULL || _buffer1 == NULL){
    DEBUG("No more RAM for sampler buffer!");
    return false;
  } else {
    DEBF("%d Bytes RAM allocated for sampler buffer, &_buffer0=%#010x\r\n", BUF_NUMBER * BUF_SIZE_BYTES, _buffer0);
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
  if (!_active || _queued) {
    _queued                 = false;
    _sampleFile             = smpFile;
    // _amplitude              = 0.0f;
    _lackingL               = 0.0f;
    _lackingR               = 0.0f;
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
    if (smpFile.size == 0) {
      _divFileSize          = 0.001f;
    } else {
      _divFileSize          = one_div((float)smpFile.size);
    }
    _lastSectorRead         = smpFile.sectors[_curChain].first - 1; 
    _midiNote = midiNote;
    _midiVelo = midiVelo; 
    _amp = (float)_midiVelo * 0.00003f * MIDI_NORM;
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
  } else {
    _queued = true;
    _nextFile = smpFile;
    _nextNote = midiNote;
    _nextVelo = midiVelo;
    AmpEnv.end(Adsr::END_FAST);
  }
  //taskEXIT_CRITICAL(&noteonMux);
}


void Voice::end(bool hard){
  if (hard) {
  //  DEBF("VOICE: hard off note %d\r\n", _midiNote);
    _active = false;
    // _amplitude = 0.0;
    AmpEnv.end(Adsr::END_NOW);
  } else  if (!_pressed && !(*_sustain)) {
  //  DEBF("VOICE: soft off note %d\r\n", _midiNote); 
    AmpEnv.end(Adsr::END_REGULAR);
  }
}


void Voice::getSample(float& sampleL, float& sampleR) {
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
      taskENTER_CRITICAL(&eventMux);
        if (_queued) {
          start(_nextFile, _nextNote, _nextVelo);
        } else {
          _active = false;
        }
      taskEXIT_CRITICAL(&eventMux);
      // _amplitude = 0.0f;
      //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
      return;
    } else {
      switch (_sampleFile.bit_depth) {
        case 24:
          switch (_sampleFile.channels) {
            case 1:                               // 24 bit mono
              //todo
              break;
            case 2:                               // 24 bit stereo
            default:
              pos = (uint32_t)_offset + (uint32_t)_smpSize * (uint32_t)_bufPosSmp[_idToPlay ];  // pos in a byte buffer
              // DEBF("pos = %d\r\n", pos);
              switch (_bufPosSmp[_idToPlay ]) {
                case 0:
                  switch (_offset) {
                    case 2:
                      l1 = _lackingL;
                      r1 = (int16_t)( static_cast<uint16_t>(_playBuf8[0])     | static_cast<uint16_t>(_playBuf8[1])<<8 );
                      l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[3])     | static_cast<uint16_t>(_playBuf8[4])<<8 );
                      r2 = (int16_t)( static_cast<uint16_t>(_playBuf8[6])     | static_cast<uint16_t>(_playBuf8[7])<<8 );
                      break;
                    case 4:
                      l1 = (int16_t)( static_cast<uint16_t>(_lackingL)        | static_cast<uint16_t>(_playBuf8[0])<<8 ); 
                      r1 = (int16_t)( static_cast<uint16_t>(_playBuf8[2])     | static_cast<uint16_t>(_playBuf8[3])<<8 );
                      l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[5])     | static_cast<uint16_t>(_playBuf8[6])<<8 );
                      r2 = (int16_t)( static_cast<uint16_t>(_playBuf8[8])     | static_cast<uint16_t>(_playBuf8[9])<<8 );
                      break;
                    case 0:
                    default:
                      l1 = _lackingL;
                      r1 = _lackingR;
                      l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+1])     | static_cast<uint16_t>(_playBuf8[pos+2])<<8 );
                      r2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+4])     | static_cast<uint16_t>(_playBuf8[pos+5])<<8 );
                  }
                  // DEBF("VOICE: POS 0 : offset: %d l1: %f l2: %f r1: %f r2: %f \r\n", _offset, l1,l2,r1,r2);
                  break;
                default:
                  l1 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos-5]) | static_cast<uint16_t>(_playBuf8[pos-4])<<8 );
                  r1 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos-2]) | static_cast<uint16_t>(_playBuf8[pos-1])<<8 );
                  l2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+1]) | static_cast<uint16_t>(_playBuf8[pos+2])<<8 );
                  r2 = (int16_t)( static_cast<uint16_t>(_playBuf8[pos+4]) | static_cast<uint16_t>(_playBuf8[pos+5])<<8 );
              }
              // if (fabs(r1-r2)>10000.0f) DEBF("pos: %d smpPos: %d fPos: %f _offset: %d \r\n", pos, _bufPosSmp[_idToPlay ], _bufPosSmpF, _offset);
              
              sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
              sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;
          } 
          bytes_played = (uint32_t)_bufPlayed * BUF_SIZE_BYTES + (uint32_t)pos + (uint32_t)_smpSize;
          
          break;
        case 16:
        default:
          switch (_sampleFile.channels) {
            case 1:                               // 16 bit mono sample file
              pos = _bufPosSmp[_idToPlay ] + _offset; 
              switch (pos) {
                case 0:                           // we will use  -1 index for interpolation
                  l1 = _lackingL;
                  l2 = _playBuf16[ 0 ];
                  break;
                default:
                  l1 = _playBuf16[ pos - 1 ];
                  l2 = _playBuf16[ pos ];
              }
              sampleL = sampleR = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
              break;
            case 2:                               // 16 bit stereo sample file
            default:
              pos = 2 * _bufPosSmp[_idToPlay ] + _offset; 
              // DEBF("pos = %d\r\n", pos);
              switch (_bufPosSmp[_idToPlay ]) {
                case 0:                           // we will use  -1 index for interpolation
                  l1 = _lackingL;
                  r1 = _lackingR;
                  l2 = _playBuf16[ 0 ];
                  r2 = _playBuf16[ 1 ];
                  // DEBF("VOICE: POS 0 : offset: %d \r\n", _offset);
                  break;
                default:
                  l1 = _playBuf16[ pos - 2 ];
                  r1 = _playBuf16[ pos - 1 ];
                  l2 = _playBuf16[ pos ];
                  r2 = _playBuf16[ pos + 1 ];
              }
              sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
              sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;
          }
          bytes_played = ((uint32_t)_bufPlayed * (uint32_t)_bufSizeSmp + (uint32_t)_bufPosSmp[_idToPlay]) * (uint32_t)_smpSize;
      }
      /*
      if ( bytes_played % 16 == 0 ) {
        _amplitude = 0.96f * (float)_amplitude + (float)sampleL + (float)sampleR;
      } 
      */
      if ( bytes_played >= _sampleFile.size ) {
        end(true);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
   //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ;//* _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ]  >= _bufSizeSmp-1 ) {
        toggleBuf();        
      }
    }
  }
}


void  Voice::feed() {
  //volatile size_t t1,t2;
  //t1 = micros();
  if (_bufEmpty[_idToFill] && !_eof) {
    int sectorsToRead = READ_BUF_SECTORS;
    int sectorsAvailable ; 
    volatile int16_t* bufAddr =  _fillBuf16;
   // DEBF("fill buf addr %d\r\n", bufAddr);
    while (sectorsToRead > 0) {
      sectorsAvailable = min(_sampleFile.sectors[_curChain].last - _lastSectorRead, (uint32_t) sectorsToRead) ;
      if (sectorsAvailable > 0) { // we have some sectors in the current chain to read
        // DEBF("block available = %d Pointer = %010x\r\n", sectorsAvailable, bufAddr);
        _Card->read_block((int16_t*)bufAddr, _lastSectorRead+1, sectorsAvailable);
        _lastSectorRead += sectorsAvailable;
        sectorsToRead -= sectorsAvailable;
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
      if (_smpSize==2 || _smpSize==4 ) _offset = _sampleFile.byte_offset / 2; // int16_t
      if (_smpSize==3 || _smpSize==6 ) _offset = _sampleFile.byte_offset;  // bytes
      _bufPosSmpF =  _bufPosSmp[_idToPlay];
    } else {
      _bufEmpty[_idToFill]    = false;
      _bufPosSmp[_idToFill]   = 0;
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
              _lackingL = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-2]) | static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-1])<<8 );
              _offset=0;
              break;
            case 1:
              _lackingL = (int16_t)( static_cast<uint16_t>(_fillBuf8[0])                | static_cast<uint16_t>(_fillBuf8[1])<<8 );
              _offset=2;
              break;
            case 2:
              _lackingL = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-1]) | static_cast<uint16_t>(_fillBuf8[0])<<8 );
              _offset=1;
          }
          _lackingR = _lackingL;
          break;
        case 2:             // 24 bit stereo
          switch (remain) {
            case 0:
              _lackingL = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-5]) | static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-4])<<8 );
              _lackingR = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-2]) | static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-1])<<8 );
              _offset = 0;
              break;
            case 2:
              _lackingL = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-1])  ); 
              // we'll need 0, 2nd and 3rd bytes from a new buffer
              _offset = 4;
              break;
            case 4:
              _lackingL = (int16_t)( static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-3]) | static_cast<uint16_t>(_playBuf8[BUF_SIZE_BYTES-2])<<8 );
              // we will need  0 and 1st bytes from a new buffer
              _offset = 2;
          }
      }
      break;
    case 16:
    default:
      _offset = 0;
      switch (_sampleFile.channels){
        case 1:             // 16 bit mono
          _lackingL = _playBuf16[_bufSizeSmp - 1];
          _lackingR = _lackingL;
          break;
        case 2:             // 16 bit stereo
        default:
          _lackingL = _playBuf16[2 * _bufSizeSmp - 2];
          _lackingR = _playBuf16[2 * _bufSizeSmp - 1];  
      }
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
  _bufPosSmpF -= (float)_bufSizeSmp;
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
  if (_queued) return 0.0f; else return (float)_bufPlayed * (float)_feedScoreCoef;
}
