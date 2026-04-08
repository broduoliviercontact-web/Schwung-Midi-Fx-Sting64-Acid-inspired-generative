---
name: schwung-create-dsp-c
description: Generate the portable C engine layer for a Schwung MIDI FX module. Use when the module design is already defined and the task is to produce dsp/<module>_engine.h and dsp/<module>_engine.c with no Schwung or Move dependencies.
---

# Schwung Create DSP C

Use this skill only after bootstrap and design.

Read first:
- current `src/dsp/`
- current `src/host/`
- current `tests/`
- `CLAUDE.md`

Primary source:
- `.claude/commands/templates/create-dsp-c.md`

Follow that file as the canonical engine template.

Rules:
- portable C only
- one explicit per-instance state struct
- no Schwung or Move headers
- explicit timing model
- clear note lifecycle
- scales live in the engine, not only in UI or comments

Expected output:
- short engine summary
- full header
- full implementation
- edge-case review
