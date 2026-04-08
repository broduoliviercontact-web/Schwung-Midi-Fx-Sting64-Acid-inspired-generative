---
name: schwung-design-module
description: Design a Schwung MIDI FX module before implementation. Use to define module identity, parameter surface, module.json shape, engine split, clock strategy, state strategy, and Move interaction for a reduced V1.
---

# Schwung Design Module

Use this skill after bootstrap and before writing code.

Read first:
- `CLAUDE.md`
- `docs/MODULES.md`
- `src/module.json`

Primary source:
- `.claude/commands/schwung/design-module.md`

Follow that file as the canonical design workflow.

Design constraints:
- keep V1 intentionally small
- prefer standard Schwung UI unless custom Move UI is justified
- expose only parameters the engine can really support
- decide clock strategy explicitly
- define scale handling before coding melodic behavior

Expected output:
- proposed file tree
- full `module.json` draft
- parameter specification
- engine architecture decision
- clock strategy
- state strategy
- UX plan
