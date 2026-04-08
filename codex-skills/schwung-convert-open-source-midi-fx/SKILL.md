---
name: schwung-convert-open-source-midi-fx
description: Orchestrate or review the full conversion of an open-source MIDI effect into a Schwung MIDI FX module. Use for end-to-end porting or for coherence review across module.json, engine, host wrapper, tests, and Move UX.
---

# Schwung Convert Open Source MIDI FX

Use this skill for full port orchestration or for cross-file coherence review.

Read first:
- `.claude/commands/WORKFLOW.md`
- `CLAUDE.md`
- `docs/MODULES.md`

Primary source:
- `.claude/commands/schwung/convert-open-source-midi-fx.md`

Follow that file as the canonical phase-by-phase process.

Required phases:
- audit
- design
- engine
- host wrapper
- state and chain integration
- coherence review
- manifest finalization
- UI decision
- build and hardware validation

Use reviewer mode when files already exist and the task is to find mismatches rather than regenerate code.

Expected output:
- orchestration plan or coherence report
- specific mismatches
- recommended fixes in priority order
