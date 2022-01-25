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

git_committer_name='Copybara'
git_committer_email='copybara@pixielabs.ai'

git config --global user.name ${git_committer_name}
git config --global user.email ${git_committer_email}

pushd "${pubRepoLoc}" &> /dev/null || exit 1

typeset -A privateHashToPublicHash
mapfile -t hashMappings < <(git --no-pager log --pretty="format:%(trailers:key=GitOrigin-RevId,valueonly,separator=%x00) %H")

for hashMapping in "${hashMappings[@]}"; do
  IFS=" " read -r -a privToPub <<< "${hashMapping}"
  privHash="${privToPub[0]}"
  pubHash="${privToPub[1]}"
  privateHashToPublicHash["${privHash}"]="${pubHash}"
done

echo "Found ${#privateHashToPublicHash[@]} commit mappings."

popd &> /dev/null || exit 1

mapfile -t tagRefs < <(git show-ref --tags -d | grep -E release | grep -E '\^\{\}' | grep -E -v 'v\d+\.\d+\.\d+-' | cut -d'^' -f1 | sed 's|refs/tags/||g')

echo "Found ${#tagRefs[@]} tag refs."

for tagRef in "${tagRefs[@]}"; do
  IFS=" " read -r -a hashToTag <<< "${tagRef}"
  hash="${hashToTag[0]}"
  tag="${hashToTag[1]}"
  if [[ $tag == *"-"* ]]; then
    # Skip tags with a "-" in them since those are RCs.
    continue
  fi
  if [[ $tag != "release/"* ]]; then
    # Skip tags that are not release tags.
    continue
  fi
  if [[ $tag == "release/cloud/"* ]]; then
    if [[ $tag != "release/cloud/prod"* ]]; then
      # If this is a cloud release tag, skip non-prod tags.
      continue
    fi
  fi
  pubHash=${privateHashToPublicHash[$hash]}
  tagMessage=$(git tag -l --format='%(contents)' "${tag}")
  if [[ -n "${pubHash}" ]]; then
    pushd "${pubRepoLoc}" &> /dev/null || exit 1
    if [[ $(git tag -l "${tag}") ]]; then
      echo "Tag ${tag} already exists. Skipping."
    else
      echo "Creating and pushing tag ${tag}"
      git tag -a "${tag}" -m "${tagMessage}" "${pubHash}"
      git push origin "${tag}"
    fi
    popd &> /dev/null || exit 1
  fi
done
