#!/bin/bash -e

# This script only exists because stirling_wrapper must be run as root.
# Otherwise one would use bazel run ...

# shellcheck disable=SC1091
source utils.sh

bazel build //src/stirling:stirling_wrapper

cmd=$(bazel info bazel-bin)/src/stirling/stirling_wrapper
run_prompt_sudo "$cmd" "$@"
