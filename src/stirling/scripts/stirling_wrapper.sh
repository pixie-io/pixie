#!/bin/bash

# This script only exists because stirling_wrapper must be run as root.
# Otherwise one would use bazel run ...

set -e

bazel build //src/stirling:stirling_wrapper

cmd=$(bazel info bazel-bin)/src/stirling/stirling_wrapper

if [[ $EUID -ne 0 ]]; then
   sudo $cmd
else
   $cmd
fi
