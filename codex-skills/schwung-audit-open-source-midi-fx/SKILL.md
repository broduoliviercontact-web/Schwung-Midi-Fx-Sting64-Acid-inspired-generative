---
name: schwung-audit-open-source-midi-fx
description: Audit an open-source MIDI effect or related project for conversion into a Schwung MIDI FX module for Ableton Move. Use before coding to judge feasibility, what to keep or drop, portability class, Move fit, and reduced-V1 scope.
---

# Schwung Audit Open Source MIDI FX

Use this skill when the input is a repo, URL, README, plugin project, or behavior spec that might become a Schwung module.

Read first:
- `CLAUDE.md`
- `docs/MODULES.md`
- `.claude/commands/WORKFLOW.md`

Primary source:
- `.claude/commands/schwung/audit-open-source-midi-fx.md`

Follow that file as the canonical audit procedure.

Focus points:
- classify the source correctly
- isolate the musical core from wrapper and UI code
- judge portability and Schwung fit
- define a reduced V1 for Move
- stop if the project is a poor fit

Expected output:
- summary
- feasibility rating
- source summary
- keep versus rewrite versus discard versus simplify
- portability class
- proposed architecture
- Move UX sketch
- risk list
- recommended next step
