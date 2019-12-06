#!/bin/bash

cd "$(dirname "$0")"

# Assuming this script was run through bazel, the executable should be here.
BIN_DIR=.

# If the script was run in a stand-alone way,
# then build and set the correct directory of the binary.
if [ -z "$BUILD_WORKSPACE_DIRECTORY" ]; then
    bazel build //src/stirling:stirling_wrapper
    BIN_DIR=$(bazel info bazel-bin)
fi

# Main test run.
./scripts/kprobe_leak_test.sh "$BIN_DIR"/stirling_wrapper
