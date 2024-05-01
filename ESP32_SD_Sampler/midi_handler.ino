
#include "midi_config.h"

inline void MidiInit() {
  #ifdef MIDI_VIA_SERIAL 
  
    Serial.begin( 115200, SERIAL_8N1 ); // midi port
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandleControlChange(handleCC);
    MIDI.setHandlePitchBend(handlePitchBend);
    MIDI.setHandleProgramChange(handleProgramChange);
    MIDI.begin(RECEIVE_MIDI_CHAN);

    #ifdef RGB_LED
      leds[0].setHue(100);//red
      FastLED.setBrightness(1);
      FastLED.show();
    #endif

  #endif
  #ifdef MIDI_VIA_SERIAL2
    pinMode( MIDIRX_PIN , INPUT_PULLDOWN);
    pinMode( MIDITX_PIN , OUTPUT);
    Serial2.begin( 31250, SERIAL_8N1, MIDIRX_PIN, MIDITX_PIN ); // midi port
    MIDI2.setHandleNoteOn(handleNoteOn);
    MIDI2.setHandleNoteOff(handleNoteOff);
    MIDI2.setHandleControlChange(handleCC);
    MIDI2.setHandlePitchBend(handlePitchBend);
    MIDI2.setHandleProgramChange(handleProgramChange);
    MIDI2.begin(RECEIVE_MIDI_CHAN);
  #endif

}


inline void handleNoteOn(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) {    
  
  #ifdef RGB_LED
    leds[0].setHue(random(255));
    FastLED.setBrightness(10);
    FastLED.show();
  #endif
  Sampler.noteOn(inNote, inVelocity);
}

inline void handleNoteOff(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) {  
  #ifdef RGB_LED
    FastLED.setBrightness(1);
    FastLED.show();
  #endif
  Sampler.noteOff(inNote, false);
}

inline void handleCC(uint8_t inChannel, uint8_t cc_number, uint8_t cc_value) {
  bool onoff = 0;
  switch (cc_number) { // global parameters yet set via ANY channel CCs
    case CC_SUSTAIN:
      onoff = cc_value >> 6;
      Sampler.setSustain(onoff);
      break;
    case CC_ANY_COMPRESSOR:
      break;
    case CC_ANY_DELAY_TIME:
      break;
    case CC_ANY_DELAY_FB:
      break;
    case CC_ANY_DELAY_LVL:
      break;
    case CC_ANY_RESET_CCS:
    case CC_ANY_NOTES_OFF:
    case CC_ANY_SOUND_OFF:
      break;
    case CC_ANY_REVERB_TIME:
      break;
    case CC_ANY_REVERB_LVL:
      break;
    default:
      {;}
  }
}

void handleProgramChange(uint8_t inChannel, uint8_t number) {
}

inline void handlePitchBend(uint8_t inChannel, int number) {
}
