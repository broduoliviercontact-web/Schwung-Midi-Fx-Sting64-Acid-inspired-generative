---
name: schwung-create-module-json
description: Create or update a production-ready module.json manifest for a Schwung MIDI FX module. Use when the design is known and the task is to define a compact Move-friendly parameter surface, capabilities, ui_hierarchy, and chain_params that match the actual implementation.
---

# Schwung Create Module JSON

Use this skill after the module design is fixed.

Read first:
- `src/module.json`
- `src/host/midi_fx_api_v1.h`
- `CLAUDE.md`

Primary source:
- `.claude/commands/templates/create-module-json.md`

Follow that file as the canonical manifest template.

Rules:
- keep the parameter surface compact
- use full parameter objects in `chain_params`
- keep enum order stable
- declare signed int ranges explicitly when musical
- do not invent fields or params the engine will not support

Expected output:
- short design summary
- full `module.json`
- assumptions made where the spec was ambiguous
