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

if [ "$#" -ne 1 ]; then
  echo "This script requires exactly one argument with the location to the checkout for the public repo."
  exit 1
fi
pubRepoLoc=$1

pushd "${pubRepoLoc}" || exit 1

typeset -A privateHashToPublicHash
mapfile -t hashMappings < <(git --no-pager log --pretty="format:%(trailers:key=GitOrigin-RevId,valueonly,separator=%x00) %H")

for hashMapping in "${hashMappings[@]}"; do
  IFS=" " read -r -a privToPub <<< "${hashMapping}"
  privHash="${privToPub[0]}"
  pubHash="${privToPub[1]}"
  privateHashToPublicHash["${privHash}"]="${pubHash}"
done

popd || exit 1

mapfile -t tagRefs < <(git show-ref --tags -d | grep -E release | grep -E '\^\{\}' | grep -E -v 'v\d+\.\d+\.\d+-' | cut -d'^' -f1 | sed 's|refs/tags/||g')

for tagRef in "${tagRefs[@]}"; do
  IFS=" " read -r -a hashToTag <<< "${tagRef}"
  hash="${hashToTag[0]}"
  tag="${hashToTag[1]}"
  pubHash=${privateHashToPublicHash[$hash]}
  tagMessage=$(git tag -l --format='%(contents)' "${tag}")
  if [[ -n "${pubHash}" ]]; then
    pushd "${pubRepoLoc}" || exit 1
    if [[ ! $(git tag -l "${tag}") ]]; then
      git tag -a "${tag}" -m "${tagMessage}" "${pubHash}"
      git push origin "${tag}"
    fi
    popd || exit 1
  fi
done
