---
name: schwung-workflow
description: Choose the right Schwung MIDI FX workflow skill for the current phase of work. Use when deciding whether to bootstrap, audit, design, implement, generate templates, review coherence, build, or publish inside the SKILL-SCHWUNG-MIDI-FX repo.
---

# Schwung Workflow

Use this skill when the main need is sequencing the work rather than generating a specific file.

Primary source:
- `.claude/commands/WORKFLOW.md`

Use that file as the canonical map from task type to skill choice.

Routing guide:
- start or change context: `schwung-repo-bootstrap`
- audit an external project: `schwung-audit-open-source-midi-fx`
- define the module: `schwung-design-module`
- write manifest: `schwung-create-module-json`
- write engine: `schwung-create-dsp-c`
- write wrapper: `schwung-create-host-wrapper`
- write tests: `schwung-create-test`
- review implementation: `schwung-implement-native-midi-fx`
- design Move controls: `schwung-build-move-ui-and-controls`
- review full coherence: `schwung-convert-open-source-midi-fx`
- build and deploy: `schwung-build-and-install`
- package and publish: `schwung-publish-custom-module`

Core principle:
- keep this as a reduced V1
- prefer a stable Schwung-native design over feature parity
