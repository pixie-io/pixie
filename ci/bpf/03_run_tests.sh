#!/bin/bash -ex

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

printenv
git config --global --add safe.directory /pl/src/px.dev/pixie

# 4 means that tests not present.
# 38 means that bes update failed.
# Both are not fatal.
check_retval() {
  if [[ $1 -eq 0 || $1 -eq 4 || $1 -eq 38 ]]; then
    echo "Bazel returned ${1}, ignoring..."
  else
    echo "Bazel failed with ${1}"
    exit "${1}"
  fi
}

cp ci/bes-gce.bazelrc bes.bazelrc

IFS=' '
# Read the environment variable and set it to an array. This allows
# us to use an array access as args.
read -ra BAZEL_ARGS <<< "${BAZEL_ARGS}"

bazel build "${BAZEL_ARGS[@]}" --target_pattern_file "${BUILDABLE_FILE}"
check_retval $?

bazel test "${BAZEL_ARGS[@]}" --target_pattern_file "${TEST_FILE}"
check_retval $?

rm -rf bazel-testlogs-archive
mkdir -p bazel-testlogs-archive
cp -a bazel-testlogs/ bazel-testlogs-archive || true

STASH_FILE="${STASH_NAME}.tar.gz"
mkdir -p .archive && tar --exclude=.archive  -czf ".archive/${STASH_FILE}" bazel-testlogs-archive/**
gsutil -o GSUtil:parallel_composite_upload_threshold=150M cp ".archive/${STASH_FILE}" "gs://${GCS_STASH_BUCKET}/${BUILD_TAG}/${STASH_FILE}"
