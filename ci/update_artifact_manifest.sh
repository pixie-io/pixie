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

versions_file="$(realpath "${VERSIONS_FILE:?}")"
repo_path="$(git rev-parse --show-toplevel)"
manifest_bucket="${ARTIFACT_MANIFEST_BUCKET:-pixie-dev}"
manifest_path="manifest.json"

manifest_updates="$(mktemp)"
# For now we manually change the versions_file into an array of ArtifactSets instead of an individual artifact set.
# This avoids changing versions_gen.
jq '[.]' < "${versions_file}" > "${manifest_updates}"

bazel run //src/utils/artifacts/manifest_updater -- --manifest_bucket="${manifest_bucket}" --manifest_path="${manifest_path}" --manifest_updates="${manifest_updates}"

rm "${manifest_updates}"
