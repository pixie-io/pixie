#!/usr/bin/env bash
# shellcheck disable=SC2086,SC2207
# Original script form bazel repo as shown below.
# Changes include:
#   Make the build type configurable.
#   Add target pattern selection instead of just //...
#
# Copyright 2015 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# This script looks at the files changed in git against origin/master
# (actually a common ancestor of origin/master and the current commit) and
# queries for all build and test targets associated with those files.
#
# Running this script on a CI server should allow you to only test the targets
# that have changed since the last time your merged or fast forwarded.
#
# This script can be used to recreate the benefits that TAP provides to
# Google's developers as describe by Mike Bland on his article on Google's
# infrastructure.
# https://mike-bland.com/2012/10/01/tools.html#tools-tap-sponge
#
# "Every single change submitted to Google's Perforce depot is built and
# tested, and only those targets affected by a particular change are
# built and tested"
#
# When this script is triggered by Gerrit's patchset-updated hook (for example)
# you can replace origin/master in the COMMIT_RANGE variable initialization
# with the branch passed as argument to the hook. When using Jenkins with the
# Gerrit Trigger Plugin, use $GERRIT_BRANCH instead. This would make it
# possible to have the Verified label on Gerrit patchsets populated as fast
# as possible.
# For a ref-updated event, use "${GERRIT_OLDREV}..${GERRIT_NEWREV}" as the
# value for COMMIT_RANGE.
# When running in Travis-CI, you can directly use the $TRAVIS_COMMIT_RANGE
# environment variable.
set -x

COMMIT_RANGE=${COMMIT_RANGE:-$(git merge-base origin/main HEAD)".."}
TARGET_PATTERN=${TARGET_PATTERN:-"//..."}
BAZEL_RUN_EXTRA_ARGS=
BAZEL_QUERY_EXTRA_ARGS=

if [ -n "${COMPILATION_MODE}" ]; then
  BAZEL_RUN_EXTRA_ARGS="${BAZEL_RUN_EXTRA_ARGS} --compilation_mode=${COMPILATION_MODE}"
fi

if [ -n "${CONFIG}" ]; then
  BAZEL_RUN_EXTRA_ARGS="${BAZEL_RUN_EXTRA_ARGS} --config=${CONFIG}"
fi

# Go to the root of the repo
cd "$(git rev-parse --show-toplevel)" || exit

# Get a list of the current files in package form by querying Bazel.
files=()
for file in $(git diff --name-only ${COMMIT_RANGE} ); do
  files+=($(bazel query ${BAZEL_QUERY_EXTRA_ARGS} --noshow_progress $file))
done


# Query for the associated buildables
buildables=$(bazel query \
    ${BAZEL_QUERY_EXTRA_ARGS} \
    --keep_going \
    --noshow_progress \
    "kind(.*_binary, rdeps(${TARGET_PATTERN}, set(${files[*]})))")
# Run the tests if there were results
if [[ -n $buildables ]]; then
  echo "Building binaries"
  bazel build ${BAZEL_RUN_EXTRA_ARGS} $buildables
  rc=$?
  if [ $rc -ne 0 ]; then
    exit $rc
  fi
fi

tests=$(bazel query \
    ${BAZEL_QUERY_EXTRA_ARGS} \
    --keep_going \
    --noshow_progress \
    "kind(test, rdeps(${TARGET_PATTERN}, set(${files[*]}))) except attr('tags', 'manual', //...)")
# Run the tests if there were results
if [[ -n $tests ]]; then
  echo "Running tests"
  bazel test ${BAZEL_RUN_EXTRA_ARGS} $tests
  rc=$?
  if [ $rc -eq 4 ]; then
    # Exit code 4 says no tests found, which can happen with filters.
    exit 0
  fi
  exit $rc
fi
