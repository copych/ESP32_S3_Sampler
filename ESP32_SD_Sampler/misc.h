#pragma once

//#define DEBUG_ON
//#define DEBUG_CORE_TIME


#define INI_FILE "sampler.ini"


#define SACRIFY_VOICES        1          // voices used for smooth transisions to avoid clicks

#define DMA_NUM_BUF 2
#define DMA_BUF_LEN 32
#define READ_BUF_SECTORS 7      // that many sectors (assume 512 Bytes) per read operation, the more, the faster it reads
#define FASTLED_INTERNAL        // remove annoying pragma messages

const float MIDI_NORM           = (1.0f / 127.0f);

#define SAMPLE_RATE 44100
const float DIV_SAMPLE_RATE = 1.0f/(float)(SAMPLE_RATE);

//#define C_MAJOR_ON_START             // play C major chord on startup (testing)

#if defined(CONFIG_IDF_TARGET_ESP32S3)
//  #define RGB_LED         38      // RGB LED as a vital sign
  #define MIDIRX_PIN      4       // this pin is used for input when MIDI_VIA_SERIAL2 defined (note that default pin 17 won't work with PSRAM)
  #define MIDITX_PIN      9       // this pin will be used for output (not implemented yet) when MIDI_VIA_SERIAL2 defined
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

#ifdef ARDUINO_USB_CDC_ON_BOOT
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT USBSerial
#endif

//#define MIDI_VIA_SERIAL       // use this option to enable Hairless MIDI on Serial port @115200 baud (USB connector), THIS WILL BLOCK SERIAL DEBUGGING as well
#define MIDI_VIA_SERIAL2        // use this option if you want to operate by standard MIDI @31250baud, UART2 (Serial2), 

// 1.0594630943592952645618252949463 // is a 12th root of 2 (pitch increase per semitone)
// 1.05952207969042122905182367802396 // stretched tuning (plus 60 cents per 7 octaves)
// converts semitones to speed: -12.0 semitones becomes 0.5, +12.0 semitones becomes 2.0
static __attribute__((always_inline)) inline float semitones2speed(float semitones) {
 // return powf(2.0f, semitones * 0.08333333f);
  return powf(1.059463f, semitones);
}

// fast but inaccurate approximation ideas taken from here:
// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/

static __attribute__((always_inline)) inline float fastPow(float a, float b) {
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

static __attribute__((always_inline)) inline float fast_semitones2speed(float semitones) {
  return fastPow(2.0f, semitones * 0.08333333f);
}

static __attribute__((always_inline)) inline float fclamp(float in, float min, float max){
  return fmin(fmax(in, min), max);
}

static __attribute__((always_inline)) inline float one_div(float a) {
    float result;
    asm volatile (
        "wfr f1, %1"          "\n\t"
        "recip0.s f0, f1"     "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "rfr %0, f0"          "\n\t"
        : "=r" (result)
        : "r" (a)
        : "f0","f1","f2"
    );
    return result;
}

int strpos(char *hay, char *needle, int offset)
{
   char haystack[strlen(hay)];
   strncpy(haystack, hay+offset, strlen(hay)-offset);
   char *p = strstr(haystack, needle);
   if (p)
      return p - haystack+offset;
   return -1;
}

#ifdef MIDI_VIA_SERIAL
  #undef DEBUG_ON
#endif

// debug macros
#ifdef DEBUG_ON
  #define DEB(...) USBSerial.print(__VA_ARGS__) 
  #define DEBF(...)  USBSerial.printf(__VA_ARGS__)
  #define DEBUG(...) USBSerial.println(__VA_ARGS__)
#else
  #define DEB(...)
  #define DEBF(...)
  #define DEBUG(...)
#endif

// lookup tables
#define TABLE_BIT            5UL           // bits per index of lookup tables. 10 bit means 2^10 = 1024 samples. Values from 5 to 11 are OK. Choose the most appropriate.
#define TABLE_SIZE            (1<<TABLE_BIT)        // samples used for lookup tables (it works pretty well down to 32 samples due to linear approximation, so listen and free some memory)
#define TABLE_MASK          (TABLE_SIZE-1)        // strip MSB's and remain within our desired range of TABLE_SIZE
#define DIV_TABLE_SIZE      (1.0f/(float)TABLE_SIZE)
#define CYCLE_INDEX(i)        (((int32_t)(i)) & TABLE_MASK ) // this way we can operate with periodic functions or waveforms with auto-phase-reset ("if's" are pretty CPU-costly)
#define SHAPER_LOOKUP_MAX     5.0f                  // maximum X argument value for tanh(X) lookup table, tanh(X)~=1 if X>4 
#define SHAPER_LOOKUP_COEF    ((float)TABLE_SIZE / SHAPER_LOOKUP_MAX)
#define TWOPI 6.2831853f
#define ONE_DIV_TWOPI 0.159154943f 

static const float sin_tbl[TABLE_SIZE+1] = {
  0.000000000f,   0.195090322f,  0.382683432f,  0.555570233f,  0.707106781f,  0.831469612f,  0.923879533f,  0.980785280f,
  1.000000000f,   0.980785280f,  0.923879533f,  0.831469612f,  0.707106781f,  0.555570233f,  0.382683432f,  0.195090322f, 
  0.000000000f,  -0.195090322f, -0.382683432f, -0.555570233f, -0.707106781f, -0.831469612f, -0.923879533f, -0.980785280f, 
  -1.000000000f, -0.980785280f, -0.923879533f, -0.831469612f, -0.707106781f, -0.555570233f, -0.382683432f, -0.195090322f, 0.000000000f };

static const float shaper_tbl[TABLE_SIZE+1] {
  0.000000000f, 0.154990730f, 0.302709729f, 0.437188785f, 0.554599722f, 0.653423588f, 0.734071520f, 0.798242755f, 
  0.848283640f, 0.886695149f, 0.915824544f, 0.937712339f, 0.954045260f, 0.966170173f, 0.975136698f, 0.981748725f, 
  0.986614298f, 0.990189189f, 0.992812795f, 0.994736652f, 0.996146531f, 0.997179283f, 0.997935538f, 0.998489189f, 
  0.998894443f, 0.999191037f, 0.999408086f, 0.999566912f, 0.999683128f, 0.999768161f, 0.999830378f, 0.999875899f , 0.999909204f };

inline float lookupTable(const float (&table)[TABLE_SIZE+1], float index ) { // lookup value in a table by float index, using linear interpolation
  static float v1, v2, res;
  static int32_t i;
  static float f;
  i = (int32_t)index;
  f = (float)index - i;
  v1 = (table)[i];
  v2 = (table)[i+1];
  res = (float)f * (float)(v2-v1) + v1;
  return res;
}

inline float fast_shape(float x) {
    float sign = 1.0f;
    if (x < 0) {
      x = -x;
      sign = -1.0f;
    }
    if (x >= 4.95f) {
      return sign; // tanh(x) ~= 1, when |x| > 4
    }
  return  sign * lookupTable(shaper_tbl, (x*SHAPER_LOOKUP_COEF)); // lookup table contains tanh(x), 0 <= x <= 5
}

inline void tend_to(float &x, const float &target, float rate) {
  x = (float)x * (float)rate + (1.0f - (float)rate) * ((float)target - (float)x);
}

inline float fast_sin(const float x) {
  const float argument = ((x * ONE_DIV_TWOPI) * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline float fast_cos(const float x) {
  const float argument = ((x * ONE_DIV_TWOPI + 0.25f) * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline void fast_sincos(const float x, float* sinRes, float* cosRes){
  *sinRes = fast_sin(x);
  *cosRes = fast_cos(x);
}

// taken from here: https://www.esp32.com/viewtopic.php?t=10540#p43343
// probably it's fixed already, so keep it here just in case
inline float divsf(float a, float b) {
    float result;
    asm volatile (
        "wfr f0, %1\n"
        "wfr f1, %2\n"
        "div0.s f3, f1 \n"
        "nexp01.s f4, f1 \n"
        "const.s f5, 1 \n"
        "maddn.s f5, f4, f3 \n"
        "mov.s f6, f3 \n"
        "mov.s f7, f1 \n"
        "nexp01.s f8, f0 \n"
        "maddn.s f6, f5, f3 \n"
        "const.s f5, 1 \n"
        "const.s f2, 0 \n"
        "neg.s f9, f8 \n"
        "maddn.s f5,f4,f6 \n"
        "maddn.s f2, f9, f3 \n" /* Original was "maddn.s f2, f0, f3 \n" */
        "mkdadj.s f7, f0 \n"
        "maddn.s f6,f5,f6 \n"
        "maddn.s f9,f4,f2 \n"
        "const.s f5, 1 \n"
        "maddn.s f5,f4,f6 \n"
        "maddn.s f2,f9,f6 \n"
        "neg.s f9, f8 \n"
        "maddn.s f6,f5,f6 \n"
        "maddn.s f9,f4,f2 \n"
        "addexpm.s f2, f7 \n"
        "addexp.s f6, f7 \n"
        "divn.s f2,f9,f6\n"
        "rfr %0, f2\n"
        :"=r"(result):"r"(a), "r"(b)
    );
    return result;
}
