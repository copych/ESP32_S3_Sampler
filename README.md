# ESP32-S3 SD Sampler
ESP32-S3 SD Sampler is a polyphonic music synthesizer, which can play PCM WAV samples directly from an SD (microSD) card connected to an ESP32-S3.
Simple: one directory = one instrument. Plain text "sampler.ini" manages how samples to be spread over the clavier.
The main difference, comparing to the projects available on the net, is that this sampler WON'T try to preload all the stuff into the RAM/PSRAM to play it on demand. So it's not limited in this way by the size of the memory chip and can take really huge (per-note true sampled multi-velocity several gigabytes) sample sets. It only requires that the card is freshly formatted FAT32 and has no or very few bad blocks (actually it requires that the WAV files is written with little or no fragmentation at all). On start it analyzes existing file allocation table (FAT) and forms it's own sample lookup table to be able to access data immediately, using SDMMC with a 4-bit wide bus.

# Is it possible to run on ESP32 not S3?
For the time being the sampler only supports S3 variant. 
It's possible to rearrange classes in order to allocate members dynamically, and run it on an ESP32, but ESP32 has a strong limitation: memory segmentation (SRAM0, SRAM1, SRAM2 and segments within these parts like BSS, IRAM, DRAM etc) is hardware defined and has different performance. So it's quite a challenge to fit all the objects and buffers in appropriate memory regions. I have tried and managed to compile, but the performance was much worse so I rolled back. If someone would like to, please fork the repository and try.

# Polyphony
Current setting is 19 voices. Combined limitation is per-voice buffer size (i.e. how many sectors we read per request to the SD). The more the size, the more the speed. But the more the size, the more memory we need.

# Velocity layers
There are currently 16 velocity layers (i.e. dynamic variants of each sampled note) which corresponds to the maximum count that I have found (https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html).


# Audio FX
Reverb is implemented as a POC (Proof of Concept). Probably there are some idle CPU cycles left which we can turn into chorus or whatever.

# TODO
The state of the project has not gone far from POC, so don't you expect a ready-made perfect sampler.
What's not working:
* Hairless MIDI doesn't work, at least for my h/w setup (LOLIN S3 Pro)
* Velocity routines mostly not implemented.
* Envelope params are hard-coded
* SAMPLER.INI syntax has to be developed much further. Now it only supports basic piano-like melodic WAV-sets.
* Only a simple reverb effect is implemented as a POC with hard-coded params.
* On polyphony overrun there are some audible clicks. We need to retrig voices in a more delicate way.
* It lacks schematics, but, please, check the .h files for the connectivity info.

# SAMPLER.INI example
here is an example of a sampler.ini file for the Salamander Grand Piano WAV-set:
```
melodic=<NAME><OCTAVE>v<VELO:1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16>
normalized=false
enveloped=true
# For example, with this template 
# the file named F#4v7.wav will be treated as F# of the 4th octave
# played with 7th velocity of 1 through 16 possible.
# 
# Everything is case insensitive. Spaces are processed as valid chars.
# Only WAV files are processed.
# 
# Sample file name template to parse:
# Parsed fields:
# <NAME> - note name in sharp (#) or flat(b) notation: both Eb and D# are valid
# <OCTAVE> - octave number
# <VELO:a,b,c...z> - velocity layers from lowest to highest, comma separated
# <NUMBER> - parsed, but not used - some digits
```
One should put the sampler.ini file to the same folder where the corresponding WAV files are stored. 
