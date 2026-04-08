---
name: schwung-midi-fx
description: Build, port, review, test, and install Schwung MIDI FX modules for Ableton Move. Use when working in the SKILL-SCHWUNG-MIDI-FX repo, when adapting an external MIDI effect into a Schwung module, or when debugging/building/installing a Move `dsp.so`.
---

# Schwung MIDI FX

Use this skill as the entry point for `/Users/supervie/Documents/max-skill-test/SKILL-SCHWUNG-MIDI-FX`.

This repo now exposes separate Codex skills under `codex-skills/`. Prefer those direct skills when the task is specific.

Use:
- `schwung-workflow` to choose the right phase
- `schwung-repo-bootstrap` to start or switch context
- `schwung-audit-open-source-midi-fx` to evaluate a source project
- `schwung-design-module` to define the module before coding
- `schwung-create-module-json` to draft or revise the manifest
- `schwung-create-dsp-c` to generate the portable engine
- `schwung-create-host-wrapper` to generate the Schwung wrapper
- `schwung-create-test` to generate native tests
- `schwung-implement-native-midi-fx` to improve existing code
- `schwung-build-move-ui-and-controls` for Move controls and UI decisions
- `schwung-convert-open-source-midi-fx` for full-port orchestration or coherence review
- `schwung-build-and-install` for build, deploy, and smoke-test work
- `schwung-publish-custom-module` for packaging and release

Global rules still apply:
- keep the strict engine versus wrapper split
- prefer a reduced V1 over feature parity
- run native tests before any Move install
- treat `get_param` formatting and note lifecycle as critical

Core references:
- `.claude/commands/WORKFLOW.md`
- `CLAUDE.md`
- `BUILDING.md`
- `docs/MODULES.md`
