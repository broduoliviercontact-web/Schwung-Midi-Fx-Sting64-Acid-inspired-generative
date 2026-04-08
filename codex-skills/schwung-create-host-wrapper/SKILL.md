---
name: schwung-create-host-wrapper
description: Generate the Schwung host wrapper for a MIDI FX module. Use when the portable engine and manifest are known and the task is to implement host/<module>_plugin.c with correct plugin API wiring, MIDI dispatch, parameter parsing, state save/load, and safe note lifecycle.
---

# Schwung Create Host Wrapper

Use this skill after the engine API and parameter surface are defined.

Read first:
- `src/host/`
- `src/host/midi_fx_api_v1.h`
- `src/host/plugin_api_v1.h`
- `src/dsp/<module>_engine.h`
- `src/module.json`

Primary source:
- `.claude/commands/templates/create-host-wrapper.md`

Follow that file as the canonical host-wrapper template.

Non-negotiable rules:
- support raw, float-formatted, and normalized Move param strings
- implement matching `set_param` and `get_param` paths for all manifest params
- return `snprintf(...)` from `get_param`
- keep save and load aligned with editable parameters
- flush active notes before reset or mode changes

Expected output:
- short wrapper summary
- full `host/<module>_plugin.c`
- self-review
