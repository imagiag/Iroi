# Iroi tweaks

A fork of Befaco's Iroi patch (upstream: `upstream` remote, stock source tagged `stock`)
with extra features. Every build is in `releases/`, named after the feature it adds,
listed in `releases/RELEASES.md` and tagged `iroi-vN` in git.

## Features

### Resonator V/Oct (v1)

The resonator can follow a pitch CV, 1 V/octave, on the resonator CV input (the jack
that modulates Resonator Tune/Feedback).

- **Turn it on:** hold SHIFT and turn the Resonator Feedback knob (it had no SHIFT
  function). Left half = the original resonator, right half = V/Oct.
  Then long-press MAP to save as usual, or it's back to original at the next power-up.
- **Scale trim:** where the knob sits in the right half fine-tunes the volts-per-octave
  scale, ±5 %. 3 o'clock is the nominal scale. To set it, play C2 then C4 and turn
  until the two are exactly two octaves apart. Only needed once per module.
- **Tune knob:** transposes, C1 to C5 with 0 V (C3 at noon), as in the original mode.
- **Chord:** the three poles keep fixed intervals above the note (root, octave,
  octave + fifth, close to the original chord with Tune at noon), so the whole chord
  transposes. **Dissonance** (SHIFT + Tune) still spreads each pole's left/right
  channels apart, as in the original mode.
- **What changes in V/Oct mode:** the CV no longer modulates Feedback (it's the pitch
  now), and the CV amounts for Tune/Feedback are ignored. MOD and random still work
  on Tune. Feedback, Volume and everything else behave as before.
- **Range:** in tune from ~20 Hz to ~3 kHz. Poles that would go past those limits drop
  or rise an octave.

**Measured on the computer** (`make -C test`, the real resonator code pinged note by note):
within ±1 cent from 20 Hz to 1.3 kHz, and within 2 cents up to 1.6 kHz at every
Feedback setting (low Feedback goes a few cents flat above that, where it barely
rings). Without the compensation the original delay maths would be up to 2.8
semitones flat at the top. How well it tracks on the module also depends on the CV
input's real scaling, which is what the trim is for.

How the tracking works: the delay length for each note subtracts the delay of the
filters inside the resonator's feedback loop (the lowpass, and the DC blocker, whose
corner is moved from ~38 Hz down to ~0.8 Hz in V/Oct mode so low notes stay
harmonic). See `VoctPitch.h`.

## Install, and going back to stock

Patch updates leave the module's own firmware, calibration and saved settings alone.

1. Open <https://pingdynasty.github.io/OwlWebControl/extended.html> in Chrome
   (Web MIDI), with Iroi connected over USB.
2. Load a `.syx` from `releases/` and store it, the same way as Befaco's official
   updates.
3. **To go back to stock**, do the same with `releases/iroi-stock-1.0.1.syx` (Befaco's
   official 1.0.1 patch, unchanged).

Saved settings work both ways. The V/Oct setting sits in a spare slot of the saved
settings file that the stock patch doesn't read. If you save from the stock patch,
that slot is cleared, so the tweaked patch then starts in the original mode and you
set the trim again.

## Building

```bash
git submodule update --init --recursive
```

```bash
tools/release.sh "feature label" "what it adds"
```

The release script runs the host tests first, then builds with OwlProgram
(`lib/OwlProgram`, platform OWL3) and the ARM GCC 13.3 toolchain. It packs the
`.syx` with `tools/owlsyx.py`, which matches OwlProgram's FirmwareSender byte for
byte on Befaco's official file.

Host tests on their own:

```bash
make -C test
```

```bash
make -C test regression
```

`regression` builds the original resonator mode from this tree and from the stock
source, and checks that the two render sample-for-sample identical output.
