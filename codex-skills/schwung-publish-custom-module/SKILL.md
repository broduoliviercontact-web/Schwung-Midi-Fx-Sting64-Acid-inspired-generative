---
name: schwung-publish-custom-module
description: Package and publish a finished Schwung MIDI FX module as an installable custom module release. Use when build and tests already pass and the goal is tarball packaging, release.json, GitHub release workflow, version tags, and shareable release assets.
---

# Schwung Publish Custom Module

Use this skill only after the module already works and the release version is intentional.

Read first:
- `src/module.json`
- `README.md`
- `scripts/build.sh`
- `scripts/install.sh`
- `.github/workflows/` if present
- `scripts/package.sh` if present
- `release.json` if present

Primary source:
- `.claude/commands/schwung/publish-custom-module.md`

Follow that file as the canonical release workflow.

Critical rules:
- package under a top-level `<module_id>/` folder
- asset name must derive from `module_id`
- tag must be `v<version>`
- `release.json` version and download URL must match the release asset
- do not continue if build or tests fail

Expected output:
- packaging plan
- required release files
- verification of tarball structure
- release commands or workflow updates
