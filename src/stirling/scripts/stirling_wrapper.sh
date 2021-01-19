#!/bin/bash -e

# This script only exists because stirling_wrapper must be run as root.
# Otherwise one would use bazel run ...

script_dir=$(dirname "$0")

# shellcheck source=./src/stirling/scripts/utils.sh
source "$script_dir"/utils.sh

bazel build //src/stirling/binaries:stirling_wrapper

cmd=$(bazel info bazel-bin)/src/stirling/binaries/stirling_wrapper
run_prompt_sudo "$cmd" "$@"
