#include "voice.h"

bool Voice::allocateBuffers(eMemType_t mem_type) {
  // actually this RAM/PSRAM works no more, cause malloc() can now decide to allocate in PSRAM
  mem_type = MEM_RAM ; // PSRAM is not fast enough ((((
  if (mem_type == MEM_RAM ) {
    // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
      _buffer0 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES , MALLOC_CAP_INTERNAL);
      _buffer1 = (int16_t*)heap_caps_malloc( BUF_SIZE_BYTES , MALLOC_CAP_INTERNAL);
      if( _buffer0 == NULL || _buffer1 == NULL){
        DEBUG("No more RAM for sampler buffer!");
        return false;
      } else {
        DEBF("%d Bytes RAM allocated for sampler buffer, &_buffer0=%#010x\r\n", BUF_NUMBER * BUF_SIZE_BYTES, _buffer0);
      }
  } else {
      _buffer0 = (int16_t*)ps_malloc( BUF_SIZE_BYTES);
      _buffer1 = (int16_t*)ps_malloc( BUF_SIZE_BYTES);
      if( _buffer0 == NULL || _buffer1 == NULL){
        DEBUG("No more PSRAM for sampler buffer!");
        return false;
      } else {
        DEBF("%d Bytes PSRAM allocated for sampler buffer, &_buffer0=%#010x\r\n", BUF_NUMBER * BUF_SIZE_BYTES, _buffer0);
      }
  } 
  return true;
}

void Voice::init(SDMMC_FAT32* Card, bool* sustain){
  _Card = Card;
  _sustain = sustain;
#ifdef BOARD_HAS_PSRAM
  if (!allocateBuffers(MEM_PSRAM)) {
    DEBUG("NOT ENOUGH MEMORY");
    delay(100);
    while(1){;}
    }
#else
  if (!allocateBuffers(MEM_RAM)) {
    DEBUG("NOT ENOUGH MEMORY");
    delay(100);
    while(1){;}
    }
#endif
  AmpEnv.Init(SAMPLE_RATE);
  AmpEnv.End(true);
  _active = false;
}

void Voice::prepare(const sample_t& smp) {
  _sampleFile             = smp;
  _amplitude              = 0.0f;
  _lackingL               = 0.0f;
  _lackingR               = 0.0f;
  _smpSize                = smp.channels * smp.bit_depth / 8;
  _speed                  = smp.speed;
  _bufSizeSmp             = BUF_SIZE_BYTES / _smpSize;
  _bufEmpty[0]            = true;
  _bufEmpty[1]            = true;
  _bufPosSmp[0]           = _bufSizeSmp;
  _bufPosSmp[1]           = _bufSizeSmp;
  _eof                    = false; 
  _bufPlayed              = 0;
  _fillBuf                = _buffer0;
  _playBuf                = _buffer1;
  _idToFill               = 0;
  _idToPlay               = 1;
  _curChain               = 0;
  if (smp.size == 0) {
    _divFileSize          = 0.001f;
  } else {
    _divFileSize          = 1.0f / (float)smp.size;
  }
  _lastSectorRead         = smp.sectors[_curChain].first - 1;
  // DEBF("VOICE: prepare velo_layer %d ch %d bits %d\r\n",smp.orig_velo_layer, smp.channels, smp.bit_depth );
}

inline void Voice::toggleBuf(){  
  _bufEmpty[_idToPlay ] = true;
  switch (_sampleFile.channels){
  case 1:
    _lackingL = _playBuf[_bufSizeSmp - 1];
    _lackingR = _lackingL;
  case 2:
    _lackingL = _playBuf[2 * _bufSizeSmp - 2];
    _lackingR = _playBuf[2 * _bufSizeSmp - 1];  
  }
  switch(_idToPlay ) { 
    case 0:
      _playBuf = _buffer1;
      _idToPlay = 1;
      _fillBuf = _buffer0;
      _idToFill = 0;
      break;
    default:
      _playBuf = _buffer0;
      _idToPlay = 0;
      _fillBuf = _buffer1;
      _idToFill = 1;
  }
//  DEBF("&playBuf=%#010x &fillBuf=%#010x\r\n", _buffer0, _fillBuf);
  _bufPosSmpF -= (float)_bufSizeSmp;
  _bufPlayed++;
} 

inline void Voice::start(uint8_t midiNote, uint8_t midiVelo){
  _midiNote = midiNote;
  _midiVelo = midiVelo; 
  AmpEnv.Retrigger(true);
  _active = true;
  _pressed = true;
 // DEBF("VOICE: start note %d velo %d\r\n", midiNote, midiVelo);
}

void Voice::end(bool hard){
  if (hard) {
    DEBF("VOICE: hard off note %d\r\n", _midiNote);
    _active = false;
    _amplitude = 0.0;
    AmpEnv.End(hard);
  } else  if (!_pressed && !(*_sustain)) {
  //  DEBF("VOICE: soft off note %d\r\n", _midiNote);
//    _active = false; // remove
    AmpEnv.End(hard);
  }
}

void Voice::getSample(float& sampleL, float& sampleR) {
  sampleL = 0.0f;
  sampleR = 0.0f;
  if (_bufEmpty[0] && _bufEmpty[1]) return;
  int pos;
  float env;
  size_t bytes_played;
  float l1, l2, r1, r2;
  if (_active) {
    env =  (float)AmpEnv.Process() * 0.00003f;
   // env = 1.0;
    if (!AmpEnv.IsRunning()) {
      _active = false;
      _amplitude = 0.0f;
    //  DEBF("Voice::getSample: note %d active=false\r\n", _midiNote);
    } else {
      switch (_sampleFile.channels) {
        case 1:                               // mono sample file
          pos = _bufPosSmp[_idToPlay ]; 
          switch (pos) {
            case 0:                           // we will use  -1 index for interpolation
              l1 = _lackingL;
              l2 = _playBuf[ 0 ];
              break;
            default:
              l1 = _playBuf[ pos - 1 ];
              l2 = _playBuf[ pos ];
          }
          sampleL = 0.00003f * interpolate( l1, l2, _bufPosSmpF ) * env;
          sampleR = sampleL;
          break;
        case 2:                               // stereo sample file
          pos = 2 * _bufPosSmp[_idToPlay ]; 
         // DEBF("pos = %d\r\n", pos);
          switch (pos) {
            case 0:                           // we will use  -1 index for interpolation
              l1 = _lackingL;
              r1 = _lackingR;
              l2 = _playBuf[ 0 ];
              r2 = _playBuf[ 1 ];
              break;
            default:
              l1 = _playBuf[ pos - 2 ];
              r1 = _playBuf[ pos - 1 ];
              l2 = _playBuf[ pos ];
              r2 = _playBuf[ pos + 1 ];
          }
          sampleL = (float)interpolate( l1, l2, _bufPosSmpF ) * (float)env;
          sampleR = (float)interpolate( r1, r2, _bufPosSmpF ) * (float)env;
          break;
        default:
          // must not be possible
          sampleL = 0.0f;
          sampleR = 0.0f;
      }
      bytes_played = (_bufPlayed * _bufSizeSmp + _bufPosSmp[_idToPlay]) *_smpSize;
  //    if ( bytes_played % 16 == 0) _amplitude = 0.98f * (float)_amplitude + (float)sampleL + (float)sampleR;
      if ( bytes_played >= _sampleFile.size ) {
        end(true);
       // DEBF("VOICE: bytes played >= size\r\n");
      }
   //   DEBF("Played %d bytes\r\n", bytes_played);
      _bufPosSmpF += (float)_speed ;//* _speedModifier;
      _bufPosSmp[_idToPlay ] = _bufPosSmpF;
      if (_bufPosSmp[_idToPlay ] >= _bufSizeSmp) {
        toggleBuf();        
      }
    }
  }
}

void  Voice::feed() {
  volatile size_t t1,t2;
  t1 = micros();
  if (_bufEmpty[_idToFill] && !_eof) {
    int sectorsToRead = READ_BUF_SECTORS;
    int sectorsAvailable ; 
    volatile int16_t* bufAddr =  _fillBuf;
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
    if (_bufEmpty[0] && _bufEmpty[1]) { // init state: bufToFill = 0, bufToPlay = 1, buffer0 is filled now, we must switch buffers manually without playing
      _bufEmpty[_idToFill]    = false;
      _bufPosSmp[_idToFill]   = 0;
      _idToFill               = 1;
      _fillBuf                = _buffer1;
      _idToPlay               = 0;
      _playBuf                = _buffer0;
      _bufPosSmp[_idToFill]   = _bufSizeSmp;
      _bufPosSmp[_idToPlay]   = sizeof(wav_header_t) / _smpSize; // skip wav header in buffer
      _playBuf[_sampleFile.channels * _bufPosSmp[_idToPlay] - 1] = 0;
      _playBuf[_sampleFile.channels * _bufPosSmp[_idToPlay] - 2] = 0;
      _bufPosSmpF =  _bufPosSmp[_idToPlay];
    } else {
      _bufEmpty[_idToFill]    = false;
      _bufPosSmp[_idToFill]   = 0;
    }
  }
  t2 = micros();
  //DEBF("Fill time %d\r\n", t2-t1);
}

uint32_t Voice::hunger() {
    if (!_active || _eof) return 0;
    return (/*(float)_speedModifier * */(float)_speed * (float)(_bufPosSmp[0] + _bufPosSmp[1])) * (float)_smpSize ; // the bigger the value, the sooner we empty the buffer
}

// linear interpolation of 2 neighbour values value by float index (mantissa only used)
inline float Voice::interpolate(float& v1, float& v2, float index) {
  float res;
  int32_t i = (int32_t)index;
  float f = (float)index - i;
  res = (float)f * (float)(v2 - v1) + v1;
  return res;
}
