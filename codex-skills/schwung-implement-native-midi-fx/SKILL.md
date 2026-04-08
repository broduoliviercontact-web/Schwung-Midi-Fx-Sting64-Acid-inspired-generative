---
name: schwung-implement-native-midi-fx
description: Implement or review the native C engine and host behavior of a Schwung MIDI FX module. Use for existing or new module code when the goal is safe MIDI behavior, explicit note lifecycle, robust parameter parsing, correct get_param formatting, and reliable Move integration.
---

# Schwung Implement Native MIDI FX

Use this skill when writing or revising native module code.

Read first:
- `CLAUDE.md`
- `docs/MODULES.md`
- `src/module.json`
- current `src/dsp/*`
- current `src/host/*`
- current `tests/*`

Primary source:
- `.claude/commands/schwung/implement-native-midi-fx.md`

Follow that file as the canonical implementation and review guide.

Non-negotiable rules:
- keep engine code portable and separate from host code
- make note lifecycle explicit and safe
- parse raw, float-formatted, and normalized Move param values
- return `snprintf(...)` from `get_param`
- do not call Move clock APIs in free-running modules

Expected output:
- engine summary
- instance state
- MIDI rules
- functions to implement
- edge cases
- code
- self-review
