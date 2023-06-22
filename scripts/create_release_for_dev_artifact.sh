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

repo="pixie-io/dev-artifacts"

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <release-name> <version> [<artifact>...]"
  exit 1
fi

release_name="$1"
version="$2"
artifacts=( "${@:3}" )

tag_name="${release_name}/${version}"

repo_dir="$(mktemp -d)"
git clone git@github.com:pixie-io/dev-artifacts.git "${repo_dir}"

pushd "${repo_dir}" &> /dev/null
git tag "${tag_name}"
git push origin "${tag_name}"
popd &> /dev/null
rm -rf "${repo_dir}"

gh release create --repo="${repo}" "${tag_name}" --title "${release_name} ${version}" --notes ""
gh release upload --repo="${repo}" "${tag_name}" "${artifacts[@]}"
