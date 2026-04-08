---
name: schwung-build-move-ui-and-controls
description: Design the Ableton Move control surface for a Schwung MIDI FX module. Use to decide knob mapping, shift behavior, pads, step buttons, LEDs, and whether custom ui.js or ui_chain.js is justified instead of the standard Schwung parameter UI.
---

# Schwung Build Move UI And Controls

Use this skill only after the parameter surface is known.

Read first:
- `src/module.json`
- `CLAUDE.md`

Primary source:
- `.claude/commands/schwung/build-move-ui-and-controls.md`

Follow that file as the canonical UI design workflow.

Default bias:
- standard UI first
- custom `ui.js` only when the interaction truly requires it
- custom `ui_chain.js` only when chain editing needs a tighter surface

Focus points:
- direct knobs own the most musical controls
- shift is optional, not mandatory
- LEDs convey state, not decoration
- pads and step buttons need a clear musical job

Expected output:
- interaction summary
- knob map
- shift layer
- pad map
- step-button map
- LED plan
- UI file decision
- implementation notes
