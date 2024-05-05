# ESP32-S3 SD Sampler
ESP32-S3 SD Sampler is a polyphonic music synthesizer, which can play PCM WAV samples directly from an SD (microSD) card connected to an ESP32-S3.
Simple: one directory = one instrument. Plain text "sampler.ini" manages how samples to be spread over the clavier.
The main difference, comparing to the projects available on the net, is that this sampler WON'T try to preload all the stuff into the RAM/PSRAM to play it on demand. So it's not limited in this way by the size of the memory chip and can take really huge (per-note true sampled multi-velocity several gigabytes) sample sets. It only requires that the card is freshly formatted FAT32 and has no or very few bad blocks (actually it requires that the WAV files is written with little or no fragmentation at all). On start it analyzes existing file allocation table (FAT) and forms it's own sample lookup table to be able to access data immediately, using SDMMC with a 4-bit wide bus.

# Is it possible to run on ESP32 not S3?
For the time being the sampler only supports S3 variant. 
It's possible to rearrange classes in order to allocate members dynamically, and run it on an ESP32, but ESP32 has a strong limitation: memory segmentation (SRAM0, SRAM1, SRAM2 and segments within these parts like BSS, IRAM, DRAM etc) is hardware defined and has different performance. So it's quite a challenge to fit all the objects and buffers in appropriate memory regions. I have tried and managed to compile, but the performance was much worse so I rolled back. If someone would like to, please fork the repository and try.

# Dependencies
* Fixed string library https://github.com/fatlab101/FixedString
* Arduino MIDI library https://github.com/FortySevenEffects/arduino_midi_library
* If you want to use RGB LEDs, then also FastLED library is needed https://github.com/FastLED/FastLED

# Polyphony
With the microSD cards that I have, the current setting is 19 stereo voices. I now set 8 sectors per read, which gives approx. 5 MB/s reading speed. Combined limitation is per-voice buffer size (i.e. how many sectors we read from the SD per request). The more the size, the more the speed. But the more the size, the more memory we need. In theory, 5 MB/s at 44100 Hz 16 bit stereo should give 29 voices polyphony, so there is probably a room to improve to get more simultaneous voices. But the limitation can also be caused by the computing power and by the internal cache performance.

PS. Of what I have tested, faster cards won't give you dramatical improvement in the matter of polyphony. I have tried a newer microSD which reads 8 sectors random blocks at apx. 7 MB/s, but only 20 voices I have managed to run.

# Velocity layers
There are currently 16 velocity layers (i.e. dynamic variants of each sampled note) which corresponds to the maximum count that I have found (https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html).

# Audio FX
Reverb is implemented as a POC (Proof of Concept). Probably there are some idle CPU cycles left which we can turn into chorus or whatever.

# TODO
The state of the project has not gone far from POC, so don't you expect a ready-made perfect sampler.
What's not working:
* On polyphony overrun there are some audible clicks, it should retrig voices in a more delicate way.
* Hairless MIDI doesn't work on LOLIN S3 Pro, but works on a generic board.
* Velocity routines are mostly NOT implemented.
* <s>Envelope params are hard-coded</s> (PARTLY MOVED TO INI)
* SAMPLER.INI syntax has to be developed much further. Now it only supports basic piano-like melodic WAV-sets. (WORK IN PROGRESS, syntax changed, drum kits supported now)
* Only a simple reverb effect is implemented as a POC with hard-coded params.
* It lacks schematics, but, please, check the .h files for the connectivity info.
* 24 and 32 bit samples support (24 BIT STEREO IS SUPPORTED NOW, BUT ONLY MSB 16 BITS MATTER YET)
* Demo video shall be done

# SAMPLER.INI examples
One should put the sampler.ini file to the same folder where the corresponding WAV files are stored. 

Here is an example of a sampler.ini file for the Salamander Grand Piano WAV-set:
```
[sampleset]
title = Salamander Grand Piano
; type=percussive/melodic
type=melodic
; Are all the samples of equal loudness? If true then we apply amplification according to the midi note velocity.
normalized=true
; Are the samples already amp-enveloped?
enveloped=true


[filename]
# Filename elements recognized:
; <NAME> - note name in sharp (#) or flat(b) notation, i.e. both Eb and D# are valid
; <OCTAVE> - octave number
; <NUMBER> - parsed, but not used for now: i.e. some numbers initially used for naming, sorting or whatever
; <VELO> velocity layer
; <INSTR> instruments names (mostly percussion) used in filenames, initially they are collected from this ini file
; elements without brackets will be treated as some constant string delimeters

; these elements are case insensitive, heading and trailing spaces are trimmed.
template=<NAME><OCTAVE>v<VELO>

; we must provide these variants along with the template. The order is important: from the most quiet to the most loud, comma separated
veloVariants = 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16

[range]
first = F#6
last = G8
noteoff = false

# [note] sections 
; instr=instrument_name(as in filename)
; noteoff=0/1 (0=ignore, 1=end note)
; speed=float_number(1.0 means unchanged (default), 1.2 means 20% faster which is higher pitch, 0.9 is 10% lower)


[envelope]
default = true
; times in seconds
attackTime = 0.0
decayTime = 0.05
releaseTime = 12.0

; sustain level 0.0 - 1.0
sustainLevel = 1.0
```

another sampler.ini example is for a drumkit MuldjordKit-SFZ-20201018, all the samples moved to a single folder
```
[sampleset]
title = Generic Drum Kit

; type=percussive/melodic
; different technics used while spreading samples over the clavier
; percussive type allows per-note fx-send level
type=percussive

; Are all the samples of equal loudness? If true then we apply amplification according to the midi note velocity.
normalized=true

; Are the samples already amp-enveloped?
enveloped=true

[envelope]
default = true

; times in seconds
attackTime = 0.0
decayTime = 0.05
releaseTime = 12.0

; sustain level 0.0 - 1.0
sustainLevel = 1.0


[filename]
# Filename elements recognized:
; <NAME> - note name=name in sharp (#) or flat(b) notation, i.e. both Eb and D# are valid
; <OCTAVE> - octave number
; <NUMBER> - parsed, but not used for now: i.e. some numbers initially used for naming, sorting or whatever
; <VELO> velocity layer
; <INSTR> instruments names (mostly percussion) used in filenames, initially they are collected from this ini file
; only instrument names mentioned in this ini (in [note] or [range] sections) will be recognized 
; elements without brackets will be treated as some constant string delimeters
; these elements are case insensitive, heading and trailing spaces are trimmed.
; template should be specified first in this section
template=<VELO>-<INSTR>

; we must provide these variants. The order is important: from the most quiet to the most loud, comma separated
veloVariants = 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16


[group]
notes = F#1,A#1,G#1

[group]
notes = G#2,A2

[group]
notes = C#2,D2


# [note] sections
; name = notename in sharp (#) or flat(b) notation, i.e. both Eb and D# are valid
; instr = instrument_name(as in filename)
; noteoff = 0/1 (0 is 'ignore', 1 is 'end note'), also yes/no, true/false are recognized
; speed = float_number (1.0 means unchanged (default), 1.2 means 20% faster which is higher pitch, 0.9 is 10% lower)
; only instrument names mentioned in this ini (in [note] or [range] sections) will be recognized 

[note]
name=b0
instr=kdrumL
noteoff=false

[note]
name=c1
instr=kdrumR
noteoff=no

[note]
name=C#1
instr=SnareRest
noteoff=0

[note]
name=D1
instr=Snare
noteoff=yes

[note]
name=D#1
; assuming we have "#-SnareRest (2).WAV" files in the drumkit folder after copying all the files into a single dir.
instr=SnareRest (2)
noteoff=true

[note]
name=E1
; assuming we have "#-Snare (2).WAV" files in the drumkit folder after copying all the files into a single dir.
instr=snare (2)
noteoff=0

[note]
name=F1
instr=Tom1
noteoff=0

[note]
name=G1
instr=Tom2
noteoff=0

[note]
name=A1
instr=Tom3
noteoff=0

[note]
name=B1
instr=Tom4
noteoff=0

[note]
name=C2
instr=Tom4
noteoff=0
speed=1.15

[note]
name=F#1
instr=HiHatClosed
noteoff=0

[note]
name=A#1
instr=HiHatOpen
noteoff=1

[note]
name=C#2
instr=CrashL
noteoff=0

[note]
name=D2
instr=CrashL
noteoff=true

[note]
name=E2
instr=China
noteoff=0

[note]
name=G#2
instr=CrashR
noteoff=0

[note]
name=A2
instr=CrashR
noteoff=yes

[note]
name=D#2
instr=RideL
noteoff=0

[note]
name=B2
instr=RideR
noteoff=0

[note]
name=F2
instr=RideLBell
noteoff=0

[note]
name=A#2
instr=RideRBell
noteoff=0
```
