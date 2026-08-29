# aphex_beat.py

A detour from the c project: a generator for an Aphex Twin style drill'n'bass beat.

Running `python3 aphex_beat.py` (needs numpy) produces two files from one shared score:

- `aphex_beat.mid` - a 3-track MIDI file meant for importing into a DAW like FL Studio.
  Drag it into the playlist and pick "import as one channel per track": you get a drums
  track (GM mapping, channel 10), a 303-style acid bassline and a music-box lead.
  Slides in the acid line are written as overlapping notes- put a mono/portamento synth
  on that track (3xOsc or Transistor Bass in FL) and the overlaps become 303 glides.
- `aphex_beat.wav` - a 25 second stereo preview rendered with a small softsynth in the
  script, so you can hear how the MIDI is intended to sound without opening a DAW.

The piece is 16 bars at 170 BPM in A minor:

| bars  | whats going on |
|-------|----------------|
| 1-4   | breakbeat plus the 303 acid line |
| 5-8   | music-box lead enters, bar 8 ends in an accelerating snare roll |
| 9-10  | breakdown- drums thin out, acid and delay carry it |
| 11-16 | second acid pattern, lead up an octave, final roll walks up the toms into a crash |

The drum programming is the interesting part: a base breakbeat is laid per bar, then
"rush zones" are chosen and filled with snare retriggers- 3 to 16 hits per zone with
velocity ramps and hard left/right ping-pong panning, which is the signature drill'n'bass
move. The two big rolls use a geometric series for the hit spacing so they accelerate.
Everything random goes through a seeded RNG, so every run reproduces the exact same beat.

The preview synth is deliberately simple: sine-sweep kick, filtered-noise snare and hats,
an FM bell for the lead, and for the acid a naive saw through a resonant state variable
filter with an envelope on the cutoff- accented notes push the envelope harder, which is
what makes a 303 line sound angry.
