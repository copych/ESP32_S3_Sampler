#include <Arduino.h>
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
volatile int out_buf_id = 0;
volatile int gen_buf_id = 1;
static float sampler_l[2][DMA_BUF_LEN];     // sampler L buffer
static float sampler_r[2][DMA_BUF_LEN];     // sampler R buffer
static float mix_buf_l[2][DMA_BUF_LEN];     // mix L channel
static float mix_buf_r[2][DMA_BUF_LEN];     // mix R channel
int16_t out_buf[2][DMA_BUF_LEN * 2];        // i2s L+R output buffer
portMUX_TYPE eventMux = portMUX_INITIALIZER_UNLOCKED; 
/*
hw_timer_t * timer1 = NULL;                 // Timer variables
hw_timer_t * timer2 = NULL;                 // Timer variables
portMUX_TYPE timer1Mux = portMUX_INITIALIZER_UNLOCKED; 
portMUX_TYPE timer2Mux = portMUX_INITIALIZER_UNLOCKED; 
volatile boolean timer1_fired = false;
volatile boolean timer2_fired = false;

// =============================================================== Timer interrupt handler ===============================================================
void IRAM_ATTR onTimer1() {
   portENTER_CRITICAL_ISR(&timer1Mux);
   timer1_fired = true;
   portEXIT_CRITICAL_ISR(&timer1Mux);
}

void IRAM_ATTR onTimer2() {
   portENTER_CRITICAL_ISR(&timer2Mux);
   timer2_fired = true;
   portEXIT_CRITICAL_ISR(&timer2Mux);
}
*/

// =============================================================== forward declarations ===============================================================
static inline void IRAM_ATTR mixer() ;
static inline void IRAM_ATTR i2s_output ();
static inline void IRAM_ATTR sampler_generate_buf();

// =============================================================== PER CORE TASKS ===============================================================
static void IRAM_ATTR audio_task(void *userData) { // core 0 task
  DEBUG ("core 0 audio task run");
  vTaskDelay(10);
  volatile size_t t1,t2,t3,t4;
  out_buf_id = 0;
  gen_buf_id = 1;
  while (true) {
    t1=micros();
    sampler_generate_buf();
    t2=micros();
    mixer(); 
    t3=micros();
    i2s_output();
    t4=micros();
  //  DEBF("gen=%d, mix=%d, output=%d\r\n", t2-t1, t3-t2, t4-t3);
    out_buf_id = 1;
    gen_buf_id = 0;
 
    t1=micros();
    sampler_generate_buf();
    t2=micros();
    mixer(); 
    t3=micros();
    i2s_output();
    t4=micros();
  //  DEBF("gen=%d, mix=%d, output=%d\r\n", t2-t1, t3-t2, t4-t3);
    out_buf_id = 0;
    gen_buf_id = 1;
  }
}
 
static void  control_task(void *userData) { // core 1 task
  DEBUG ("core 1 control task run");
  vTaskDelay(10);
  static uint32_t passby = 0;
  while (true) { 
    /*
    if (timer2_fired) {
      timer2_fired = false;
      timeClick();
    }
    */      
    if (passby % 16 == 0) {      
      processButtons();
    //  DEBF("ControlTask unused stack size = %d bytes\r\n", uxTaskGetStackHighWaterMark(ControlTask));
    //  DEBF("SynthTask unused stack size = %d bytes\r\n", uxTaskGetStackHighWaterMark(SynthTask));
    }
    passby++;
    

    Sampler.fillBuffer();
    
    #ifdef MIDI_VIA_SERIAL
      MIDI.read();
    #endif

    
    #ifdef MIDI_VIA_SERIAL2
      MIDI2.read();
    #endif
    
    Sampler.fillBuffer();
    
    taskYIELD();
  }
}

// =============================================================== SETUP() ===============================================================
void setup() {
  
#ifdef DEBUG_ON
  USBSerial.begin(115200);
#endif

delay(100);

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
  
  Sampler.setCurrentFolder(1);

DEBUG("I2S: INIT");
  i2sInit();
  
  initButtons();
  
  xTaskCreatePinnedToCore( audio_task, "SynthTask", 4000, NULL, 23, &SynthTask, 0 );
 
  xTaskCreatePinnedToCore( control_task, "ControlTask", 6000, NULL, 10, &ControlTask, 1 );

  // setting timer interrupt
  /*
  timer1 = timerBegin(0, 80, true);               // Setup timer for midi
  timerAttachInterrupt(timer1, &onTimer1, true);  // Attach callback
  timerAlarmWrite(timer1, 4000, true);            // 4000us, autoreload
  timerAlarmEnable(timer1);
  */
  /*
  timer2 = timerBegin(1, 80, true);               // Setup general purpose timer
  timerAttachInterrupt(timer2, &onTimer2, true);  // Attach callback
  timerAlarmWrite(timer2, 500000, true);          // microseconds, autoreload
  timerAlarmEnable(timer2);
  DEBUG ("Timer(s) started");
  */
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
