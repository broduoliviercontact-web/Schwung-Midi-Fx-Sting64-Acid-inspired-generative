---
name: schwung-repo-bootstrap
description: Bootstrap work in the SKILL-SCHWUNG-MIDI-FX repo before designing, modifying, porting, or debugging a Schwung MIDI FX module. Use at the start of a session to gather the required docs, inspect the current engine and wrapper split, and produce an implementation brief before coding.
---

# Schwung Repo Bootstrap

Use this skill for `/Users/supervie/Documents/max-skill-test/SKILL-SCHWUNG-MIDI-FX`.

Read first:
- `CLAUDE.md`
- `BUILDING.md`
- `docs/MODULES.md`
- `docs/PROJECT_PLAN.md`
- `docs/PORTING_NOTES.md`
- `src/module.json`

Then inspect:
- `src/dsp/*_engine.h`
- `src/dsp/*_engine.c`
- `src/host/*_plugin.c`
- `src/host/midi_fx_api_v1.h`
- `src/host/plugin_api_v1.h`
- `tests/*_engine_test.c`
- `tests/*_midi_fx_test.c`

Primary source:
- `.claude/commands/schwung/repo-bootstrap.md`

Use that file as the canonical workflow and follow its checklist exactly.

Non-negotiable rules:
- Do not code before the implementation brief is complete.
- Keep the strict engine versus host-wrapper split.
- Do not call Move clock APIs in free-running designs.
- Treat `get_param` formatting as critical.

Expected output:
- one repo-aware summary paragraph
- the implementation brief
- a proposed file tree
- the next files to inspect or create
