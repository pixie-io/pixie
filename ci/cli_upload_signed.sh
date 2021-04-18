#!/usr/bin/env bash

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

repo_path=$(bazel info workspace)

# shellcheck source=ci/gcs_utils.sh
. "${repo_path}/ci/gcs_utils.sh"

set -ex

printenv

release_tag=${TAG_NAME##*/v}

output_path="gs://pixie-prod-artifacts/cli/${release_tag}"
copy_artifact_to_gcs "$output_path" "cli_darwin_amd64" "cli_darwin_amd64"
copy_artifact_to_gcs "$output_path" "cli_darwin_amd64.zip" "cli_darwin_amd64.zip"

# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ ! "$release_tag" == *"-"* ]]; then
  output_path="gs://pixie-prod-artifacts/cli/latest"
  copy_artifact_to_gcs "$output_path" "cli_darwin_amd64" "cli_darwin_amd64"
  copy_artifact_to_gcs "$output_path" "cli_darwin_amd64.zip" "cli_darwin_amd64.zip"
fi
