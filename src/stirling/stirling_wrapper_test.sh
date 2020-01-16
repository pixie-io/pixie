#!/bin/bash -e

cd "$(dirname "$0")"

# Assuming this script was run through bazel, the executable should be here.
BIN_DIR=.

# If the script was run in a stand-alone way,
# then build and set the correct directory of the binary.
if [ -z "${BUILD_WORKSPACE_DIRECTORY}" ] && [ -z "${TEST_TMPDIR}" ]; then
    bazel build //src/stirling:stirling_wrapper_image.tar
    BIN_DIR=$(bazel info bazel-bin)/src/stirling
fi

# shellcheck disable=SC1091
source scripts/utils.sh

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

cmd="${BIN_DIR}/stirling_wrapper"
flags="--init_only"

out=$(run_prompt_sudo "$cmd" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

check_stirling_output "$out"
