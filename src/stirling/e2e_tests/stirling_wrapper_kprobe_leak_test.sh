#!/bin/bash -e

script_dir="$(dirname "$0")"
pixie_root="$script_dir"/../../..

# shellcheck source=./src/stirling/scripts/utils.sh
source "$pixie_root"/src/stirling/scripts/utils.sh

if [ -z "$BUILD_WORKSPACE_DIRECTORY" ] && [ -z "$TEST_TMPDIR" ]; then
  # If the script was run in a stand-alone way, then build and set paths.
  stirling_wrapper=$pixie_root/$(bazel_build //src/stirling:stirling_wrapper)
else
  # If the script was run through bazel, the locations are passed as arguments.
  stirling_wrapper=$1
fi

# Main test run.
"$pixie_root"/src/stirling/scripts/kprobe_leak_test.sh "$stirling_wrapper"
