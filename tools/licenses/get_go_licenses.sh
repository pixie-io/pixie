#!/bin/bash -ex

WORKSPACE="$(bazel info workspace)"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

GO_LICENSE_OUTPUT="${SCRIPT_DIR}/go_licenses.json"
MISSING_GO_LICENSES="${SCRIPT_DIR}/go_missing_licenses.json"
# pixie-labs-buildbot's Github key. See README.md for info on changing it.
GITHUB_API_KEY=$(sops --decrypt "$WORKSPACE/credentials/dev_infra/github_token.txt")

echo "Running go license:"
echo "  output file: ${GO_LICENSE_OUTPUT}"
echo "  missing licenses file: ${MISSING_GO_LICENSES}"

bazel run //third_party:glice -- \
    --github_api "${GITHUB_API_KEY}" \
    -i ui,bazel,chef,linters,scripts,skaffold,skaffold_build,third_party,docs \
    --missing_licenses_json "${MISSING_GO_LICENSES}" \
    --json_output="${GO_LICENSE_OUTPUT}" \
    --import_path="pixielabs.ai/pixielabs" \
    -p "${WORKSPACE}"
