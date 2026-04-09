# Sting64

**Acid-inspired generative MIDI sequencer for Ableton Move**

`Sting64` is a [Schwung](https://github.com/charlesvestal/schwung) `midi_fx` module for Ableton Move. It generates looping melodic patterns with scale quantization, musical rests, variable note length, deterministic reseeding, and transport sync to the Move clock.

This project is not a literal Max-for-Live port. It is a native Schwung reinterpretation of the original **STING!64 by SKINNERBOX** concept, adapted for Move's workflow, hardware transport, and compact parameter surface.

## What It Does

Sting64 is built for fast bassline and lead generation on Move:

- generate a repeatable melodic loop from a single seed
- constrain notes to a chosen scale and root
- thin the pattern with musical rests instead of uniform dropout
- vary phrase length from `1` to `64` steps
- sync to Move transport and tempo or run from internal BPM
- shape note feel with `rate`, `swing`, `gate`, `velocity`, and `chaos`

The result sits somewhere between an acid sequencer, a melodic sketchpad, and a compact idea machine.

## Features

- Native Schwung MIDI FX module for Ableton Move
- Deterministic melodic generation with `seed`
- Loop length from `1` to `64` steps
- 19 scales, from diatonic modes to pentatonic, blues, diminished, whole-tone, and chromatic
- Musical rest generation via `density`
- Internal clock or Move MIDI clock sync
- Straight, dotted, and triplet rates from `1/4D` to `1/32T`
- Adjustable note length via `gate`
- Swing, root transpose, and output velocity control

## Quick Start

1. Install `Sting64` into a Schwung MIDI FX slot on Move.
2. Put an instrument after it in the chain.
3. Start the Move transport.
4. Start with these values:
   - `Scale`: `ionian` or `minor_pent`
   - `Steps`: `8` or `16`
   - `Density`: `0.45` to `0.70`
   - `Chaos`: `0.20` to `0.45`
   - `Rate`: `1/16`
   - `Gate`: `0.35` to `0.60`
5. Adjust `Seed`, `Root`, `Scale`, `Density`, and `Gate` to fit the track.

## Sync Note

If you want `sync = move`, enable **MIDI Clock Out / MIDI Sync Out** in the Move MIDI settings. Without clock, the module will not advance correctly in Move-sync mode.

## Parameters

| Parameter | What it does |
| --- | --- |
| `root` | Transposes the melodic center from `-24` to `+24` semitones. |
| `scale` | Quantizes generated notes to one of 19 scale sets. |
| `density` | Controls how many steps speak. Lower values create more rests. |
| `chaos` | Increases pitch spread and melodic unpredictability. |
| `swing` | Delays off-beats for a looser groove. |
| `seed` | Sets the deterministic identity of the current phrase. |
| `rate` | Sets rhythmic division: dotted, straight, or triplet values. |
| `gate` | Sets note duration as a fraction of the step length. |
| `bpm` | Internal tempo when `sync = internal`. |
| `sync` | Chooses `internal` or `move` clock source. |
| `steps` | Sets the actual loop length from `1` to `64`. |
| `velocity` | Controls output note velocity. |

## Included Scales

`ionian`, `aeolian`, `dorian`, `mixolydian`, `phrygian`, `lydian`, `locrian`, `major_pent`, `minor_pent`, `major_blues`, `minor_blues`, `harmonic_minor`, `melodic_minor`, `phrygian_dominant`, `double_harmonic`, `whole_tone`, `diminished_wh`, `diminished_hw`, `chromatic`

## Installation

### Manual install on Move

```bash
make test
./scripts/build.sh
./scripts/install.sh move.local
```

If the module does not appear immediately:

```bash
ssh root@move.local 'pkill -x schwung || true; sleep 1; nohup /data/UserData/schwung/schwung >/tmp/schwung.log 2>&1 </dev/null &'
```

### Release package

This repo can also produce a release tarball for custom-module distribution:

```bash
./scripts/package.sh
```

Expected output:

- `dist/sting64/`
- `dist/sting64-module.tar.gz`

## Build From Source

```bash
make test
./scripts/build.sh native
```

Useful paths:

- `src/dsp/sting64_engine.c` — portable melodic engine
- `src/host/sting64_plugin.c` — Schwung host wrapper
- `src/module.json` — Move-facing manifest and parameter definitions

## Project Structure

```text
src/dsp/sting64_engine.h/.c
src/host/sting64_plugin.c
src/module.json
tests/sting64_engine_test.c
tests/sting64_midi_fx_test.c
```

## Credits

- **Original inspiration:** [**STING!64 by SKINNERBOX**](https://www.maxforlive.com/library/device/4260/sting-by), a Max for Live MIDI device published by `skinnerbox`
- **Original device file referenced during port analysis:** `STING!64 by SKINNERBOX.amxd`
- **Schwung framework and host APIs:** [Charles Vestal and contributors](https://github.com/charlesvestal/schwung)
- **This repository:** native Schwung/Ableton Move adaptation of the STING idea, not an official SKINNERBOX release

## Reference Notes

The original STING project is documented publicly in a few places:

- [Ableton described Sting on January 28, 2013](https://www.ableton.com/en/blog/sting-free-acid-pattern-generator-skinnerbox/) as a free acid pattern generator by Skinnerbox.
- [Max for Live lists **STING by SKINNERBOX 1.0**](https://www.maxforlive.com/library/device/4260/sting-by) as a MIDI Effect and distributes the file `STING!64 by SKINNERBOX.amxd`.

This repository was shaped by those references plus direct patch analysis from an extracted `STING!64` device, then redesigned to fit Schwung and Move rather than emulate Max for Live one-to-one.

## Acknowledgement

If you enjoy this module, check out the original work from SKINNERBOX as well as other community Schwung modules. Sting64 stands on top of that lineage while focusing on a compact, hardware-first Move workflow.
