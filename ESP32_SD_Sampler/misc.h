#pragma once

#define DEBUG_ON

#define INI_FILE "sampler.ini"

#define DMA_NUM_BUF 2
#define DMA_BUF_LEN 32
#define READ_BUF_SECTORS 7      // that many sectors (assume 512 Bytes) per read operation, the more, the faster it reads
#define FASTLED_INTERNAL        // remove annoying pragma messages

#define SAMPLE_RATE 44100
const float DIV_SAMPLE_RATE = 1.0f/(float)(SAMPLE_RATE);

//#define C_MAJOR_ON_START             // play C major chord on startup (testing)

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  //#define RGB_LED 38              // blink as a vital sign
  #define MIDIRX_PIN      4      // this pin is used for input when MIDI_VIA_SERIAL2 defined (note that default pin 17 won't work with PSRAM)
  #define MIDITX_PIN      9      // this pin will be used for output (not implemented yet) when MIDI_VIA_SERIAL2 defined
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

const float exp_curve[128] = {
0.0f,         0.001252428f, 0.002534793f, 0.003847810f, 0.005192213f, 0.006568752f, 0.007978195f, 0.009421327f, 
0.010898955f, 0.012411904f, 0.013961017f, 0.015547158f, 0.017171214f, 0.018834090f, 0.020536713f, 0.022280036f, 
0.024065029f, 0.025892689f, 0.027764037f, 0.029680116f, 0.031641995f, 0.033650769f, 0.035707560f, 0.037813515f, 
0.039969809f, 0.042177646f, 0.044438257f, 0.046752904f, 0.049122878f, 0.051549503f, 0.054034132f, 0.056578151f, 
0.059182981f, 0.061850075f, 0.064580921f, 0.067377043f, 0.070240002f, 0.073171395f, 0.076172857f, 0.079246065f, 
0.082392732f, 0.085614614f, 0.088913510f, 0.092291261f, 0.095749750f, 0.099290909f, 0.102916713f, 0.106629185f, 
0.110430398f, 0.114322472f, 0.118307580f, 0.122387944f, 0.126565842f, 0.130843606f, 0.135223623f, 0.139708335f, 
0.144300248f, 0.149001921f, 0.153815981f, 0.158745111f, 0.163792065f, 0.168959656f, 0.174250770f, 0.179668359f, 
0.185215446f, 0.190895127f, 0.196710570f, 0.202665021f, 0.208761803f, 0.215004318f, 0.221396050f, 0.227940564f, 
0.234641514f, 0.241502639f, 0.248527766f, 0.255720817f, 0.263085806f, 0.270626841f, 0.278348132f, 0.286253987f, 
0.294348818f, 0.302637142f, 0.311123583f, 0.319812878f, 0.328709875f, 0.337819540f, 0.347146955f, 0.356697326f, 
0.366475982f, 0.376488380f, 0.386740107f, 0.397236883f, 0.407984566f, 0.418989154f, 0.430256788f, 0.441793755f, 
0.453606493f, 0.465701593f, 0.478085806f, 0.490766042f, 0.503749377f, 0.517043056f, 0.530654498f, 0.544591297f, 
0.558861231f, 0.573472262f, 0.588432545f, 0.603750428f, 0.619434458f, 0.635493387f, 0.651936177f, 0.668772004f, 
0.686010261f, 0.703660569f, 0.721732777f, 0.740236969f, 0.759183472f, 0.778582858f, 0.798445952f, 0.818783840f, 
0.839607869f, 0.860929660f, 0.882761111f, 0.905114405f, 0.928002016f, 0.951436715f, 0.975431580f, 1.0f
};

const float exp_curve_rev[128] = {
1.0f,         0.975431580f, 0.951436715f, 0.928002016f, 0.905114405f, 0.882761111f, 0.860929660f, 0.839607869f, 
0.818783840f, 0.798445952f, 0.778582858f, 0.759183472f, 0.740236969f, 0.721732777f, 0.703660569f, 0.686010261f, 
0.668772004f, 0.651936177f, 0.635493387f, 0.619434458f, 0.603750428f, 0.588432545f, 0.573472262f, 0.558861231f, 
0.544591297f, 0.530654498f, 0.517043056f, 0.503749377f, 0.490766042f, 0.478085806f, 0.465701593f, 0.453606493f, 
0.441793755f, 0.430256788f, 0.418989154f, 0.407984566f, 0.397236883f, 0.386740107f, 0.376488380f, 0.366475982f, 
0.356697326f, 0.347146955f, 0.337819540f, 0.328709875f, 0.319812878f, 0.311123583f, 0.302637142f, 0.294348818f, 
0.286253987f, 0.278348132f, 0.270626841f, 0.263085806f, 0.255720817f, 0.248527766f, 0.241502639f, 0.234641514f, 
0.227940564f, 0.221396050f, 0.215004318f, 0.208761803f, 0.202665021f, 0.196710570f, 0.190895127f, 0.185215446f, 
0.179668359f, 0.174250770f, 0.168959656f, 0.163792065f, 0.158745111f, 0.153815981f, 0.149001921f, 0.144300248f, 
0.139708335f, 0.135223623f, 0.130843606f, 0.126565842f, 0.122387944f, 0.118307580f, 0.114322472f, 0.110430398f, 
0.106629185f, 0.102916713f, 0.099290909f, 0.095749750f, 0.092291261f, 0.088913510f, 0.085614614f, 0.082392732f, 
0.079246065f, 0.076172857f, 0.073171395f, 0.070240002f, 0.067377043f, 0.064580921f, 0.061850075f, 0.059182981f, 
0.056578151f, 0.054034132f, 0.051549503f, 0.049122878f, 0.046752904f, 0.044438257f, 0.042177646f, 0.039969809f, 
0.037813515f, 0.035707560f, 0.033650769f, 0.031641995f, 0.029680116f, 0.027764037f, 0.025892689f, 0.024065029f, 
0.022280036f, 0.020536713f, 0.018834090f, 0.017171214f, 0.015547158f, 0.013961017f, 0.012411904f, 0.010898955f, 
0.009421327f, 0.007978195f, 0.006568752f, 0.005192213f, 0.003847810f, 0.002534793f, 0.001252428f, 0.0f
};


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
