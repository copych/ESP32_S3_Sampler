#pragma once

#define DEBUG_ON

#define INI_FILE "sampler.ini"

#define DMA_NUM_BUF 2
#define DMA_BUF_LEN 32
#define READ_BUF_SECTORS 8      // that many sectors (assume 512 Bytes) per read operation, the more, the faster it reads
#define FASTLED_INTERNAL        // remove annoying pragma messages

#define SAMPLE_RATE 44100
const float DIV_SAMPLE_RATE = 1.0f/(float)(SAMPLE_RATE);


#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define RGB_LED 38              // blink as a vital sign
  #define MIDIRX_PIN      4      // this pin is used for input when MIDI_VIA_SERIAL2 defined (note that default pin 17 won't work with PSRAM)
  #define MIDITX_PIN      17      // this pin will be used for output (not implemented yet) when MIDI_VIA_SERIAL2 defined
  #define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
  #define I2S_DOUT_PIN    6       // to I2S DATA IN pin (DIN D DAT) 
  #define I2S_WCLK_PIN    7       // I2S WORD CLOCK pin (WCK WCL LCK)
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define MIDIRX_PIN      22      // this pin is used for input when MIDI_VIA_SERIAL2 defined (note that default pin 17 won't work with PSRAM)
  #define MIDITX_PIN      23      // this pin will be used for output (not implemented yet) when MIDI_VIA_SERIAL2 defined
  #define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
  #define I2S_DOUT_PIN    18      // to I2S DATA IN pin (DIN D DAT) 
  #define I2S_WCLK_PIN    19      // I2S WORD CLOCK pin (WCK WCL LCK)  
#endif



//#define MIDI_VIA_SERIAL       // use this option to enable Hairless MIDI on Serial port @115200 baud (USB connector), THIS WILL BLOCK SERIAL DEBUGGING as well
#define MIDI_VIA_SERIAL2        // use this option if you want to operate by standard MIDI @31250baud, UART2 (Serial2), 


// converts semitones to speed: -12.0 semitones becomes 0.5, +12.0 semitones becomes 2.0
float semitones2speed(float semitones) {
  return powf(2.0f, semitones * 0.08333333f);
}

// fast but inaccurate approximation ideas taken from here:
// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/

float fastPow(float a, float b) {
 // int32_t mc = 1072632447;
  int16_t mc = 16256;
  float ex = fabs(fabs((float)b - (int)b) - 0.5 ) - 0.5;
    union {
        float f;
        int16_t x[2];
    } u = { a };
    u.x[1] = (int16_t)(b * ((float)u.x[1] - (float)mc) + (float)mc);
    u.x[0] = 0;
    u.f *= 1.0 + 0.138 * (ex) ;
    return u.f;
}

float fast_semitones2speed(float semitones) {
  return fastPow(2.0f, semitones * 0.08333333f);
}

inline float fclamp(float in, float min, float max){
    return fmin(fmax(in, min), max);
}

// debug macros
#ifdef MIDI_VIA_SERIAL
  #undef DEBUG_ON
#endif

#ifdef DEBUG_ON
  #define DEB(...) Serial.print(__VA_ARGS__) 
  #define DEBF(...) Serial.printf(__VA_ARGS__) 
  #define DEBUG(...) Serial.println(__VA_ARGS__) 
#else
  #define DEB(...)
  #define DEBF(...)
  #define DEBUG(...)
#endif
