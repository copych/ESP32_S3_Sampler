/*
* ESP32-S3 SD Sampler is a polyphonic music synthesizer, which can play PCM WAV samples directly from an SD (microSD) 
* card connected to an ESP32-S3. Simple: one directory = one sample set. Plain text "sampler.ini" manages how samples 
* to be spread over the keyboard. The main difference, comparing to the projects available on the net, is that this 
* sampler WON'T try to preload all the stuff into the RAM/PSRAM to play it on demand. So it's not limited in this way 
* by the size of the memory chip and can take really huge (per-note true sampled multi-velocity several gigabytes) 
* sample sets. It only requires that the card is freshly formatted FAT32 and has no or very few bad blocks (actually 
* it requires that the WAV files are written with little or no fragmentation at all). On start it analyzes existing 
* file allocation table (FAT) and forms it's own sample lookup table to be able to access data immediately, using 
* SDMMC with a 4-bit wide bus.
*
*
* Libraries used:
* Fixed string library https://github.com/fatlab101/FixedString
* Arduino MIDI library https://github.com/FortySevenEffects/arduino_midi_library
* If you want to use RGB LEDs, then also FastLED library is needed https://github.com/FastLED/FastLED
*
* (c) Copych 2024, License: MIT https://github.com/copych/ESP32_S3_Sampler?tab=MIT-1-ov-file#readme
* 
* More info:
* https://github.com/copych/ESP32_S3_Sampler
*/

#include <Arduino.h>
#include "config.h"
#include "misc.h"
#include "sdmmc.h"
#include <vector>
#include "sampler.h"
#include "fx_reverb.h"
#include <MIDI.h>


#ifdef RGB_LED
#include "FastLED.h"
CRGB leds[1];
#endif

//SET_LOOP_TASK_STACK_SIZE(READ_BUF_SECTORS * BYTES_PER_SECTOR + 20000);
//SET_LOOP_TASK_STACK_SIZE(80000);

// =============================================================== MIDI interfaces ===============================================================

#ifdef MIDI_VIA_SERIAL

  struct CustomBaudRateSettings : public MIDI_NAMESPACE::DefaultSettings {
    static const long BaudRate = 115200;
    static const bool Use1ByteParsing = false;
  };

  MIDI_NAMESPACE::SerialMIDI<HardwareSerial, CustomBaudRateSettings> serialMIDI(Serial);
  MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial, CustomBaudRateSettings>> MIDI((MIDI_NAMESPACE::SerialMIDI<HardwareSerial, CustomBaudRateSettings>&)serialMIDI);

#endif

#ifdef MIDI_VIA_SERIAL2
// MIDI port on UART2,   pins 16 (RX) and 17 (TX) prohibited on ESP32, as they are used for PSRAM
struct Serial2MIDISettings : public midi::DefaultSettings {
  static const long BaudRate = 31250;
  static const int8_t RxPin  = MIDIRX_PIN;
  static const int8_t TxPin  = MIDITX_PIN;
  static const bool Use1ByteParsing = false;
};
MIDI_NAMESPACE::SerialMIDI<HardwareSerial> Serial2MIDI2(Serial2);
MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>> MIDI2((MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>&)Serial2MIDI2);
#endif


// =============================================================== MAIN oobjects ===============================================================
SDMMC_FAT32     Card;
SamplerEngine   Sampler;
FxReverb        Reverb;

// =============================================================== GLOBALS ===============================================================
TaskHandle_t SynthTask;
TaskHandle_t ControlTask;
static int DRAM_ATTR out_buf_id = 0;
static int DRAM_ATTR gen_buf_id = 1;
static float DRAM_ATTR sampler_l[2][DMA_BUF_LEN];     // sampler L buffer
static float DRAM_ATTR sampler_r[2][DMA_BUF_LEN];     // sampler R buffer
static float DRAM_ATTR mix_buf_l[2][DMA_BUF_LEN];     // mix L channel
static float DRAM_ATTR mix_buf_r[2][DMA_BUF_LEN];     // mix R channel
static int16_t DRAM_ATTR out_buf[2][DMA_BUF_LEN * 2];        // i2s L+R output buffer


// =============================================================== forward declarations ===============================================================
static  void IRAM_ATTR mixer() ;
static  void IRAM_ATTR i2s_output ();
static  void IRAM_ATTR sampler_generate_buf();

// =============================================================== PER CORE TASKS ===============================================================
static void IRAM_ATTR audio_task(void *userData) { // core 0 task
  DEBUG ("core 0 audio task run");
  vTaskDelay(20);
  volatile uint32_t t1,t2,t3,t4;
  out_buf_id = 0;
  gen_buf_id = 1;
  
  while (true) {
#ifdef DEBUG_CORE_TIME 
    t1=micros();
#endif

    sampler_generate_buf();
    
#ifdef DEBUG_CORE_TIME 
    t2=micros();
#endif

    mixer(); 
    
#ifdef DEBUG_CORE_TIME 
    t3=micros();
#endif

    i2s_output();
    
#ifdef DEBUG_CORE_TIME 
    t4=micros();
    DEBF("gen=%d, mix=%d, output=%d, total=%d\r\n", t2-t1, t3-t2, t4-t3, t4-t1);
#endif

    out_buf_id = 1;
    gen_buf_id = 0;
    
#ifdef DEBUG_CORE_TIME 
    t1=micros();
#endif

    sampler_generate_buf();
    
#ifdef DEBUG_CORE_TIME 
    t2=micros();
#endif

    mixer(); 
    
#ifdef DEBUG_CORE_TIME 
    t3=micros();
#endif

    i2s_output();
    
#ifdef DEBUG_CORE_TIME 
    t4=micros();
    DEBF("gen=%d, mix=%d, output=%d, total=%d\r\n", t2-t1, t3-t2, t4-t3, t4-t1);
#endif

    out_buf_id = 0;
    gen_buf_id = 1;
  }
}
 
static void  IRAM_ATTR control_task(void *userData) { // core 1 task
  DEBUG ("core 1 control task run");
  vTaskDelay(20);
  static uint32_t passby = 0;

  while (true) { 

    Sampler.freeSomeVoices();
    
    Sampler.fillBuffer();

    #ifdef MIDI_VIA_SERIAL
      MIDI.read();
    #endif

    #ifdef MIDI_VIA_SERIAL2
      MIDI2.read();
    #endif
    
    passby++;

    if (passby%16 == 0 ) { 
    
      processButtons();
      taskYIELD();
      
      #ifdef RGB_LED
      leds[0].setHue(Sampler.getActiveVoices()*30); 
      FastLED.show();
      #endif
      
    //  DEBF("Active voices %d of %d \r\n", Sampler.getActiveVoices(), MAX_POLYPHONY);
    //  DEBF("ControlTask unused stack size = %d bytes\r\n", uxTaskGetStackHighWaterMark(ControlTask));
    //  DEBF("SynthTask unused stack size = %d bytes\r\n", uxTaskGetStackHighWaterMark(SynthTask));
    }
  }
}

// =============================================================== SETUP() ===============================================================
void setup() {
  
#ifdef DEBUG_ON
  USBSerial.begin(115200);
#endif

delay(2000);

#ifdef RGB_LED
  FastLED.addLeds<WS2812, RGB_LED, GRB>(leds, 1);
  FastLED.setBrightness(1);
#endif
  

DEBUG("CARD: BEGIN");
  Card.begin();
  
//delay(1000);
 // Card.testReadSpeed(READ_BUF_SECTORS,8);
  
DEBUG("REVERB: INIT");
  Reverb.Init();
 
DEBUG("MIDI: INIT");
  MidiInit();
DEBUG("SAMPLER: INIT");

  Sampler.init(&Card);
  
  Sampler.setCurrentFolder(3);

DEBUG("I2S: INIT");
  i2sInit();
  
  initButtons();
  
  xTaskCreatePinnedToCore( audio_task, "SynthTask", 4000, NULL, 28, &SynthTask, 0 );
 
  xTaskCreatePinnedToCore( control_task, "ControlTask", 8000, NULL, 20, &ControlTask, 1 );

  #ifdef C_MAJOR_ON_START
  delay(100);
  Sampler.noteOn(60,100);
  Sampler.noteOn(64,100);
  Sampler.noteOn(67,100);
  delay(1000);
  Sampler.noteOff(60, false);
  Sampler.noteOff(64, false);
  Sampler.noteOff(67, false);
  #endif
  DEBUG ("Setup() DONE");
 // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
}


void loop() {

#ifdef RGB_LED
  leds[0].setHue(1); //green
  FastLED.setBrightness(1);
  FastLED.show();
#endif

  vTaskDelete(NULL);
}
