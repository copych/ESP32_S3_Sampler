#include "misc.h"
#include "sdmmc.h"
#include <vector>
#include "sampler.h"

#define RGB_LED 38
#ifdef RGB_LED
#include "FastLED.h"
CRGB leds[1];
#endif

//SET_LOOP_TASK_STACK_SIZE(READ_BUF_SECTORS * BYTES_PER_SECTOR + 20000);

TaskHandle_t SynthTask;
TaskHandle_t ControlTask;

SDMMC_FAT32 Card;

SamplerEngine Sampler ;

volatile int out_buf_id = 0;
volatile int gen_buf_id = 1;
static float sampler_l[2][DMA_BUF_LEN];     // sampler L buffer
static float sampler_r[2][DMA_BUF_LEN];     // sampler R buffer
static float mix_buf_l[2][DMA_BUF_LEN];     // mix L channel
static float mix_buf_r[2][DMA_BUF_LEN];     // mix R channel
static union {                              // a dirty trick, instead of reinterpret_cast<>(), TODO: check which is faster
  int16_t _signed[DMA_BUF_LEN * 2];
  uint16_t _unsigned[DMA_BUF_LEN * 2];
} out_buf[2];                               // i2s L+R output buffer

hw_timer_t * timer1 = NULL;            // Timer variables
hw_timer_t * timer2 = NULL;            // Timer variables
portMUX_TYPE timer1Mux = portMUX_INITIALIZER_UNLOCKED; 
portMUX_TYPE timer2Mux = portMUX_INITIALIZER_UNLOCKED; 
volatile boolean timer1_fired = false;
volatile boolean timer2_fired = false;

/*
 * Timer interrupt handler **********************************************************************************************************************************
*/

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


// forward declarations
static inline void IRAM_ATTR mixer() ;
static inline void IRAM_ATTR i2s_output ();
static inline void IRAM_ATTR sampler_generate_buf();

static void IRAM_ATTR audio_task(void *userData) { // core 0 task
  vTaskDelay(10);
  volatile size_t t1,t2,t3,t4;
  out_buf_id = 0;
  gen_buf_id = 1;
  DEBUG ("core 0 audio task run");
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
  vTaskDelay(10);
  DEBUG ("core 1 control task run");
  while (true) {
    Sampler.fillBuffer();
    if (timer2_fired) {
      timer2_fired = false;
      timeClick();
    }
  }
}

void setup() {
#ifdef DEBUG_ON
  Serial.begin(115200);
  delay(100);
#endif
  Card.begin();
  #ifdef RGB_LED
  FastLED.addLeds<WS2812, RGB_LED, GRB>(leds, 1);
  FastLED.setBrightness(1);
  #endif
  //Card.testReadSpeed(READ_BUF_SECTORS,8);

  i2sInit();
  Sampler.init(&Card);
  xTaskCreatePinnedToCore( audio_task, "SynthTask", 10000, NULL, 25, &SynthTask, 0 );
 
  xTaskCreatePinnedToCore( control_task, "ControlTask", 8000, NULL, 8, &ControlTask, 1 );
 

    // timer interrupt
  /*
  timer1 = timerBegin(0, 80, true);               // Setup timer for midi
  timerAttachInterrupt(timer1, &onTimer1, true);  // Attach callback
  timerAlarmWrite(timer1, 4000, true);            // 4000us, autoreload
  timerAlarmEnable(timer1);
  */
  
  timer2 = timerBegin(1, 80, true);               // Setup general purpose timer
  timerAttachInterrupt(timer2, &onTimer2, true);  // Attach callback
  timerAlarmWrite(timer2, 500000, true);          // 200ms, autoreload
  timerAlarmEnable(timer2);
  DEBUG ("Timer(s) started");
  DEBUG ("Setup() DONE");
}


void loop() {
  //timeClick();
  //delay(1);
  vTaskDelete(NULL);
}


void timeClick() {
  static int state = 0;
  static bool OnOff = true;
  state ++;
  switch (state) {
    case 0:
      OnOff = false;
      break;
    case 1:
      OnOff = true;
      break;
    case 2:
      OnOff = false;
      state = -1; // becomes 0 next time
      break;
  }
  const int noteMin = 40 ;
  const int noteMax = 80 ;
  const int notePoly = 5;
  const int veloMin = 10;
  const int veloMax = 127;
  static int veloStep = 7;
  static int iNoteOn = noteMin;
  static int iNoteOff = noteMin;
  static int velo = veloMin;
  if (iNoteOn < noteMin) iNoteOn = (noteMax - noteMin + 1) + iNoteOn;
  if (iNoteOn > noteMax) iNoteOn =  iNoteOn - (noteMax - noteMin + 1);
  iNoteOff = iNoteOn - notePoly;
  if (iNoteOff < noteMin) iNoteOff = (noteMax - noteMin + 1) + iNoteOff;
  if (iNoteOff > noteMax) iNoteOff =  iNoteOff - (noteMax - noteMin + 1);
 
  taskYIELD();
  Sampler.noteOn((uint8_t)iNoteOn, (uint8_t)velo);
  Sampler.noteOff((uint8_t)iNoteOff, false);
  if (OnOff) {
    #ifdef RGB_LED
    leds[0].setHue(random(255));
    FastLED.setBrightness(1);
    FastLED.show();
    #endif
    
  } else {
    #ifdef RGB_LED
    FastLED.setBrightness(0);
    FastLED.show();
    #endif

  }
//    DEBF("Note on %d @ %d , off %d\r\n", iNoteOn , velo , iNoteOff);
    velo += veloStep;
    if (velo > veloMax) {
      velo = veloMax;
      veloStep = -veloStep;
    }
    if (velo < veloMin) {
      velo = veloMin;
      veloStep = -veloStep;
    }
    iNoteOn++;
}
