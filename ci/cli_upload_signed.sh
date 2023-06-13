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

# shellcheck source=ci/artifact_utils.sh
. "${repo_path}/ci/artifact_utils.sh"

set -ex

printenv

release_tag=${TAG_NAME##*/v}

export GCS_ARTIFACT_OPTS="-h 'Content-Disposition:filename=px'"
for arch in amd64 arm64 universal
do
  upload_artifact_to_mirrors "cli" "${release_tag}" "cli_darwin_${arch}" "cli_darwin_${arch}"

  # Check to see if it's production build. If so we should also write it to the latest directory.
  if [[ ! "$release_tag" == *"-"* ]]; then
    upload_artifact_to_mirrors "cli" "latest" "cli_darwin_${arch}" "cli_darwin_${arch}"
  fi
done
unset GCS_ARTIFACT_OPTS
