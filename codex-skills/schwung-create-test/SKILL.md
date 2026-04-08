---
name: schwung-create-test
description: Generate native C tests for a Schwung MIDI FX module. Use when the engine, host wrapper, and manifest exist or are being finalized and the goal is to cover defaults, parameter round-trips, get_param correctness, note lifecycle, save/load, and Move parsing edge cases.
---

# Schwung Create Test

Use this skill when native verification needs to be added or updated.

Read first:
- `src/dsp/<module>_engine.h`
- `src/host/<module>_plugin.c`
- `src/module.json`
- current `tests/`

Primary source:
- `.claude/commands/templates/create-test.md`

Follow that file as the canonical test template.

Two required files:
- `tests/<module>_engine_test.c`
- `tests/<module>_midi_fx_test.c`

Coverage priorities:
- defaults
- setter and getter behavior
- `get_param` returns positive lengths for known keys
- save and load round-trip
- pass-through behavior
- transport stop and stuck-note safety
- raw versus float-formatted versus normalized Move param parsing

Expected output:
- short test plan summary
- full engine test
- full host-wrapper test
- coverage review
