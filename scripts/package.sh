#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MODULE_ID="${MODULE_ID:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["id"])')}"
MODULE_VERSION="${MODULE_VERSION:-$(python3 -c 'import json; print(json.load(open("src/module.json", "r", encoding="utf-8"))["version"])')}"
BUILD_MODE="${1:-auto}"
DIST_DIR="dist"
STAGE_DIR="${DIST_DIR}/${MODULE_ID}"
ASSET_PATH="${DIST_DIR}/${MODULE_ID}-module.tar.gz"
DSO_PATH="build/aarch64/dsp.so"

echo "-> Packaging ${MODULE_ID} v${MODULE_VERSION}"

if [ ! -f "${DSO_PATH}" ]; then
    echo "-> Missing ${DSO_PATH}; building for aarch64 first..."
    ./scripts/build.sh "${BUILD_MODE}"
fi

rm -rf "${STAGE_DIR}" "${ASSET_PATH}"
mkdir -p "${STAGE_DIR}"

cp src/module.json "${STAGE_DIR}/module.json"
cp "${DSO_PATH}" "${STAGE_DIR}/dsp.so"

if [ -f src/ui.js ]; then
    cp src/ui.js "${STAGE_DIR}/ui.js"
fi

if [ -f src/ui_chain.js ]; then
    cp src/ui_chain.js "${STAGE_DIR}/ui_chain.js"
fi

tar -czf "${ASSET_PATH}" -C "${DIST_DIR}" "${MODULE_ID}"

echo "OK: ${ASSET_PATH}"
tar -tzf "${ASSET_PATH}"
