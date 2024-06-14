
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

inline float midiToExpTime(uint8_t midiCC) { // converts 0-127 range to 0.0 - 1e30 exponential range with slow start and ~infinite growth at 127 (1=0.0002s, 64=2s, 92=10s, 126=80s)
  if (midiCC==0) {return 0.0;}
  if (midiCC==127) {return 1e30;}
  return (float)(0.059970546f * exp(0.058580705f*(float)midiCC - 0.184582266f) - 0.052670842f);
}

inline void handleNoteOn(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) {    
  /*
  #ifdef RGB_LED
    leds[0].setHue(random(255));
    FastLED.setBrightness(10);
    FastLED.show();
  #endif
  */
  Sampler.noteOn(inNote, inVelocity);
}

inline void handleNoteOff(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) {  
  /*
  #ifdef RGB_LED
    FastLED.setBrightness(1);
    FastLED.show();
  #endif
  */
  Sampler.noteOff(inNote, false);
}


void handleCC(uint8_t inChannel, uint8_t cc_number, uint8_t cc_value) {
  bool onoff = 0;
  float scaled;
  switch (cc_number) { // global parameters yet set via ANY channel CCs
    case CC_SUSTAIN:
      onoff = cc_value >> 6;
      Sampler.setSustain(onoff);
      break;
/*
    case CC_RESO:
      scaled = (float)cc_value * MIDI_NORM;
      DJFilter.SetResonance(scaled);
      DEBF("Set Resonance %f\r\n", scaled);
      break;
    case CC_CUTOFF:
      scaled = (float)cc_value * MIDI_NORM;
      DJFilter.SetCutoff(scaled);
      DEBF("Set Cutoff %f\r\n", scaled);
      break;
    case CC_COMPRESSOR:
      scaled = 3.0f + cc_value * 0.307081f;
      Comp.SetRatio(scaled);
      DEBF("Set Comp Ratio %f\r\n", scaled);
      break;
*/
    case CC_ENV_DECAY:
      scaled = midiToExpTime(cc_value);
      Sampler.setDecayTime(scaled);
      DEBF("SAMPLER: MIDI: Env Set Decay Time %f s\r\n", scaled);
      break;
    case CC_ENV_ATTACK:
      scaled = midiToExpTime(cc_value);
      Sampler.setAttackTime(scaled);
      DEBF("SAMPLER: MIDI: Env Set Attack Time %f s\r\n", scaled);
      break;
    case CC_ENV_RELEASE:
      scaled = midiToExpTime(cc_value);
      Sampler.setReleaseTime(scaled);
      DEBF("SAMPLER: MIDI: Env Set Release Time %f s\r\n", scaled);
      break;
    case CC_ENV_SUSTAIN:
      scaled = (float)cc_value * MIDI_NORM;
      Sampler.setSustainLevel(scaled);
      DEBF("SAMPLER: MIDI: Env Set Sustain Level %f\r\n", scaled);
      break;
    case CC_REVERB_TIME:
      scaled = (float)cc_value * MIDI_NORM;
      Reverb.SetTime(scaled);
      DEBF("SAMPLER: MIDI: Reverb Set Time %f s\r\n", scaled);
      break;
    case CC_REVERB_LVL:
      scaled = (float)cc_value * MIDI_NORM;
      Reverb.SetLevel(scaled);
      DEBF("SAMPLER: MIDI: Reverb Set Level %f s\r\n", scaled);
      break;
/*
    case CC_DELAY_TIME:
      scaled = (float)cc_value * MIDI_NORM;
      Delay.SetLength(scaled);
      DEBF("SAMPLER: MIDI: Delay Set Time %f s\r\n", scaled);
      break;
    case CC_DELAY_FB:
      scaled = (float)cc_value * MIDI_NORM;
      Delay.SetFeedback(scaled);
      DEBF("SAMPLER: MIDI: Delay Set Feedback %f s\r\n", scaled);
      break;
    case CC_DELAY_LVL:
      scaled = (float)cc_value * MIDI_NORM;
      Delay.SetLevel(scaled);
      DEBF("SAMPLER: MIDI: Deleay Set Level %f s\r\n", scaled);
      break;
    case CC_DELAY_SEND:
      scaled = (float)cc_value * MIDI_NORM;
      Sampler.setDelaySendLevel(scaled);
      DEBF("SAMPLER: MIDI: Set Send To Delay %f s\r\n", scaled);
      break;
*/
    case CC_REVERB_SEND:
      scaled = (float)cc_value * MIDI_NORM;
      Sampler.setReverbSendLevel(scaled);
      DEBF("SAMPLER: MIDI: Set Send To Reverb %f s\r\n", scaled);
      break;
    case CC_RESET_CCS:
    case CC_NOTES_OFF:
    case CC_SOUND_OFF:
      //Sampler.endAll();
      break;
  }
}

void handleProgramChange(uint8_t inChannel, uint8_t number) {
}

inline void handlePitchBend(uint8_t inChannel, int number) {
  Sampler.setPitch(number); 
}
