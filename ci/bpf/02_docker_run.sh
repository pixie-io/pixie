#!/bin/bash -e

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

MAX_SEC=300
TIMEOUT=$(($(date -u +%s) + MAX_SEC))

until docker info; do
  if [[ $(date -u +%s) -gt TIMEOUT ]]; then
    echo "Timed out waiting for docker to be available"
    exit 1;
  fi
  echo "waiting for docker"
  sleep 2;
done;

echo "Docker Version:"
docker version

PASSTHROUGH_ENV_ARGS=(
  --env "BUILDABLE_FILE=${BUILDABLE_FILE}"
  --env "TEST_FILE=${TEST_FILE}"
  --env "BAZEL_ARGS='${BAZEL_ARGS}'"
  --env "STASH_NAME=${STASH_NAME}"
  --env "GCS_STASH_BUCKET=${GCS_STASH_BUCKET}"
  --env "BUILD_TAG=${BUILD_TAG}"
)

export PX_DOCKER_RUN_AS_ROOT=True
export PX_RUN_DOCKER_EXTRA_ARGS="${PASSTHROUGH_ENV_ARGS[*]}"

./scripts/run_docker_bpf.sh ./ci/bpf/03_run_tests.sh
