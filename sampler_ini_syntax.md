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
*  enveloped = ```boolean```
*  amplify = ```float```
*  max_voices = ```integer```
*  limit_same_notes = ```integer```

## Section [FILENAME]
*  template = ```string```
*  velo_variants = ```comma separated strings```
*  velo_limits = ```comma separated integers```

## Section [ENVELOPE]
*  attack_time = ```float```
*  decay_time = ```float```
*  release_time = ```float```
*  sustain_level = ```float```

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

## Section [GROUP]
* notes = ```comma separated strings```
