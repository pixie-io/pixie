#!/bin/bash

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

versions_file="${VERSIONS_FILE}"
manifest_updates="${MANIFEST_UPDATES}"
repo_path="$(git rev-parse --show-toplevel)"

manifest_bucket="${ARTIFACT_MANIFEST_BUCKET}"
manifest_path="${ARTIFACT_MANIFEST_PATH}"

if [[ -z "${manifest_updates}" ]]; then
  if [[ -z "${versions_file}" ]]; then
    echo "Must specify one of VERSIONS_FILE or MANIFEST_UPDATES"
    exit 1
  fi
  manifest_updates="$(mktemp)"
  # For now we manually change the versions_file into an array of ArtifactSets instead of an individual artifact set.
  # This avoids changing versions_gen.
  jq '[.]' < "${versions_file}" > "${manifest_updates}"
else
  manifest_updates="$(realpath "${manifest_updates}")"
fi

updater_args=("--manifest_updates=${manifest_updates}")
if [[ -n "${manifest_bucket}" ]]; then
  manifest_path="manifest.json"
  updater_args+=("--manifest_bucket=${manifest_bucket}" )
elif [[ -n "${manifest_path}" ]]; then
  manifest_path="$(realpath "${manifest_path}")"
else
  echo "Must specify one of ARTIFACT_MANIFEST_BUCKET or ARTIFACT_MANIFEST_PATH"
  exit 1
fi
updater_args+=("--manifest_path=${manifest_path}")

bazel run //src/utils/artifacts/manifest_updater -- "${updater_args[@]}"
