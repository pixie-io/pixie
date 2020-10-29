#!/bin/bash -ex

WORKSPACE="$(bazel info workspace)"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

UI_LICENSE_PROD_OUTPUT="${SCRIPT_DIR}/ui_licenses_prod.json"
UI_LICENSE_DEV_OUTPUT="${SCRIPT_DIR}/ui_licenses_dev.json"

pushd "${WORKSPACE}/src/ui" &> /dev/null

# License check
yarn license-checker --production --json --out "${UI_LICENSE_PROD_OUTPUT}"
yarn license-checker --development --json --out "${UI_LICENSE_DEV_OUTPUT}"

popd &> /dev/null

python3 "${SCRIPT_DIR}/npm_license_extractor.py" \
    "${UI_LICENSE_PROD_OUTPUT}" \
    "${UI_LICENSE_PROD_OUTPUT}" \
    --override_license "${SCRIPT_DIR}/override_license.json"
python3 "${SCRIPT_DIR}/npm_license_extractor.py" \
    "${UI_LICENSE_DEV_OUTPUT}" \
    "${UI_LICENSE_DEV_OUTPUT}" \
    --override_license "${SCRIPT_DIR}/override_license.json"
