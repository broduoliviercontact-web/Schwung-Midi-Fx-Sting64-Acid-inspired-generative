---
name: schwung-build-and-install
description: Build, install, and verify a Schwung MIDI FX module on Ableton Move. Use after native code is ready to run tests, cross-compile dsp.so, deploy to move.local or an IP, restart Schwung, and perform smoke and hardware checks.
---

# Schwung Build And Install

Use this skill when the module is already implemented and you need the build and deploy flow.

Read first:
- `BUILDING.md`
- `src/module.json`
- `scripts/build.sh`
- `scripts/install.sh`
- `scripts/smoke_test.sh`

Primary source:
- `.claude/commands/schwung/build-and-install.md`

Follow that file as the canonical build and deployment sequence.

Order:
1. `make test`
2. `./scripts/build.sh`
3. `./scripts/install.sh [move_ip]`
4. restart Schwung
5. `./scripts/smoke_test.sh`
6. hardware validation

Troubleshooting focus:
- missing `dsp.so`
- wrong install path
- bad `get_param` returns
- chain params mismatch
- Move clock settings
