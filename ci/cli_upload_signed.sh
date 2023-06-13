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
manifest_updates="${MANIFEST_UPDATES:?}"

for arch in amd64 arm64 universal
do
  artifact_type=""
  case "${arch}" in
    amd64) artifact_type="AT_DARWIN_AMD64" ;;
    arm64) artifact_type="AT_DARWIN_ARM64" ;;
  esac
  upload_artifact_to_mirrors "cli" "${release_tag}" "cli_darwin_${arch}" "cli_darwin_${arch}" "${artifact_type}"

  # Check to see if it's production build. If so we should also write it to the latest directory.
  if [[ ! "$release_tag" == *"-"* ]]; then
    upload_artifact_to_mirrors "cli" "latest" "cli_darwin_${arch}" "cli_darwin_${arch}"
  fi
done

create_manifest_update "cli" "${release_tag}" > "${manifest_updates}"
