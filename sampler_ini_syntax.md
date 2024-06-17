# SAMPLER.INI syntax
## General
Directories in the root folder containing SAMPLER.INI file are considered as containing sample sets.
Sampler.ini file is a human readable plain text file, the syntax is as following, and is subject to develop.
Also, please, note that some parameters are parsed, but not implemented yet.

* Due to using fixed strings, a line length shouldn't exceed 252 symbols.
* Quotes are not used.
* ```Boolean``` values can be represented by the following strings: yes, no, true, false, 1, 0, y, n, none
* ```Integer``` values should be numbers with no commas or points, i.e. 127 or -1
* ```Float``` values should contain a point, e.g. 127.0 or -1.001
* ```Strings``` are used without quotes. Leading and trailing spaces are trimmed. E.g. value of ```param = a b c   ``` would be parsed as "a b c"  

## Comment lines
Comments are started with a sharp sign (#) or a semicolon (;)

## Section [SAMPLESET]
This section contains global parameters:
*  title = ```string```
*  type = ```melodic``` or ```percussive```
*  normalized = ```boolean```
*  <s>enveloped = ```boolean```</s> no longer supported
*  amplify = ```float```
*  max_voices = ```integer```
*  limit_same_notes = ```integer```
*  attack_time = ```float```
*  decay_time = ```float```
*  release_time = ```float```
*  sustain_level = ```float```
  
## Section [FILENAME]
This section describes how the WAV files are self-mapped basing on the info parsed out of their filenames
*  template = ```string```
    *  ```<NAME>``` - note name in sharp (#) or flat(b) notation, i.e. both Eb and D# are valid  
    *  ```<OCTAVE>``` - octave number 
    *  ```<MIDINOTE>``` - midi note number (either ```note name and octave``` or ```midi number``` should be used, not both of them)
    *  ```<NUMBER>``` - parsed, but not used for now: i.e. some numbers initially used for naming, sorting or whatever
    *  ```<VELO>``` - velocity layer
    *  ```<INSTR>``` - instruments names (mostly percussion) used in filenames, initially they are collected from this ini file
    *  all other elements without brackets will be treated as some constant string delimeters
    *  e.g. for the files named like ```065_F#2_Mid.wav``` we would use ```template = <NUMBER>_<NAME><OCTAVE>_<VELO>```, where ```065``` is some number (unused), ```F#2``` is the note name, ```2``` is an octave number, ```_``` are just separators, and ```Mid``` is a velocity layer 
*  velo_variants = ```comma separated strings```
    *  we list the variants that are present in the filenames e.g. ```velo_variants = Soft,Mid,Hard```  
*  velo_limits = ```comma separated integers```
    *  optional, we list the upper limits of each velocity layer e.g. ```velo_limits = 40,96,127```

## <s>Section [ENVELOPE]</s> no longer supported, moved to [sampleset]

## Sections [RANGE] and [NOTE]
*  name = ```string```
*  first = ```string```
*  last = ```string```
*  instr = ```string```
*  noteoff = ```boolean```
*  speed = ```float```
*  attack_time = ```float```
*  decay_time = ```float```
*  release_time = ```float```
*  sustain_level = ```float```
*  limit_same_notes = ```integer``` - limits number of simultaneous same notes 

## Section [GROUP]
* notes = ```comma separated strings```
