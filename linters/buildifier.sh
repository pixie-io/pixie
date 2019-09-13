#!/bin/bash

set -e

workspace=$(bazel info workspace)
buildifier_args="--path=${workspace} ${workspace}/$1"

# Run lint so that arc can get results.
bazel run --direct_run @com_github_bazelbuild_buildtools//buildifier:buildifier -- --mode=check ${buildifier_args}

# Auto-fix the build files.
bazel run --direct_run @com_github_bazelbuild_buildtools//buildifier:buildifier -- --mode=fix ${buildifier_args}
