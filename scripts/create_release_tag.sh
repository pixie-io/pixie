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

# This creates a new tag in git for the current commit.

usage() {
  echo "Usage: $0 <artifact_type> [-p] [-r] [-m] [-n]"
  echo " -p : Push the tag to Github."
  echo " -r : Create a release."
  echo " -m : Increment the major version."
  echo " -n : Increment the minor version."
  echo "Example: echo 'this is a change' | $0 cli -p -r -m"
}

parse_args() {
  if [ $# -lt 1 ]; then
    usage
  fi

  ARTIFACT_TYPE=$1
  shift

  while test $# -gt 0; do
    case "$1" in
      -p) PUSH=true
        shift
        ;;
      -r) RELEASE=true
        shift
        ;;
      -m) BUMP_MAJOR=true
        shift
        ;;
      -n) BUMP_MINOR=true
        shift
        ;;
      *)  usage ;;
    esac
  done
}

check_args() {
  if [ "$BUMP_MAJOR" = "true" ] && [ "$BUMP_MINOR" = "true" ]; then
    echo "Cannot bump both major and minor."
    exit
  fi

  if [ "$ARTIFACT_TYPE" != "cli" ] && [ "$ARTIFACT_TYPE" != "vizier" ] && [ "$ARTIFACT_TYPE" != "operator" ] && [ "$ARTIFACT_TYPE" != "cloud" ]; then
    echo "Unsupported artifact type."
    exit
  fi
}

get_bazel_target() {
  case "$ARTIFACT_TYPE" in
    cli) BAZEL_TARGET=//src/pixie_cli:px;;
    vizier) BAZEL_TARGET=//k8s/vizier:image_bundle;;
    operator) BAZEL_TARGET=//k8s/operator:image_bundle;;
    cloud) BAZEL_TARGET=//k8s/cloud:image_bundle;;
  esac
}

function semver {
  v=$1
  v=${v//\+*/}
  echo "$v"
}

function parse {
  s=$(semver "$1")
  # shellcheck disable=SC2001
  pre=$(echo "$s" | sed 's|.*\-\(.*\)|\1|')
  s=${s//-*/}

  split=$(echo "$s" | tr '.' ' ')
  echo "${split} ${pre}"
}

function bump_patch {
  read -r major minor patch pre <<< "$(parse "$1")"
  echo "$major.$minor.$((patch +1))"
}

function bump_major {
  read -r major minor patch pre <<< "$(parse "$1")"
  echo "$((major +1)).0.0"
}

function bump_minor {
  read -r major minor patch pre <<< "$(parse "$1")"
  echo "$major.$((minor +1)).0"
}

function update_pre {
  read -r major minor patch pre <<< "$(parse "$1")"
  commits=$2
  branch=$3
  echo "$major.$minor.$patch-pre-$branch.$commits"
}

function generate_changelog {
  prev_tag=$1
  bazel_target=$2

  bug_changelog=''
  feature_changelog=''
  cleanup_changelog=''

  # Find all the commits between now and the last release.
  commits=$(git log HEAD..."$prev_tag" --pretty=format:"%H")

  # Find all file dependencies of the bazel target.
  bazel query --noshow_progress 'kind("source file", deps('"$bazel_target"')) union buildfiles(deps('"$bazel_target"'))' | sed  -e 's/:/\//' -e 's/^\/\+//' > target_files.txt
  trap "rm -f target_files.txt" EXIT

  # For each commit, pull out changelog descriptions if it touches any bazel target deps.
  for commit in $commits
  do
    files=$(git show --name-only --pretty="format:" "$commit")
    for file in $files
    do
      if grep -iq "$file" target_files.txt; then
        changeType=''
        releaseNote=''

        log=$(git log --format=%B -n 1 "$commit")

        # Get the type of change (cleanup|bug|feature).
        typeRe='Type of change: /kind ([A-Za-z]+)'
        if [[ $log =~ $typeRe ]]; then
          changeType=${BASH_REMATCH[1]}
        fi

        # Get release notes.
        notesRe="\`\`\`release-note\s*(.*)\`\`\`"
        if [[ $log =~ $notesRe ]]; then
          releaseNote=${BASH_REMATCH[1]}
        fi

        declare -a cleanup_changelog
        declare -a bug_changelog
        declare -a feature_changelog

        if [[ -n $releaseNote ]]; then
          case $changeType in
          "cleanup")
            cleanup_changelog+=("$releaseNote")
            ;;
          "bug")
            bug_changelog+=("$releaseNote")
            ;;
          "feature")
            feature_changelog+=("$releaseNote")
            ;;
          *)
            ;;
          esac
        fi
        break
      fi
    done
  done

  # Output changelog.
  if [[ ${#feature_changelog[@]} != 0 ]]; then
    echo "### New Features"
    printf -- '- %s' "${feature_changelog[@]%%+[[:space]]}"
    echo ""
  fi
  if [[ ${#bug_changelog[@]} != 0 ]]; then
    echo "### Bug Fixes"
    printf -- '- %s' "${bug_changelog[@]%%+[[:space]]}"
    echo ""
  fi
  if [[ ${#cleanup_changelog[@]} != 0 ]]; then
    echo "### Cleanup"
    printf -- '- %s' "${cleanup_changelog[@]%%+[[:space]]}"
    echo ""
  fi
}

parse_args "$@"
check_args
get_bazel_target

# Fetch the latest tags.
git fetch --tags

# Get the latest release tag.
tags=$(git for-each-ref --sort='-*authordate' --format '%(refname:short)' refs/tags \
  | grep "release/$ARTIFACT_TYPE/v" | grep -v "\-")

# Get the most recent tag.
prev_tag=$(echo "$tags" | head -1)

# Parse the tag.
version_str=${prev_tag//*\/v/}

new_version_str=""
if [ "$BUMP_MAJOR" = "true" ]; then
  new_version_str=$(bump_major "$version_str")
elif [ "$BUMP_MINOR" = "true" ]; then
  new_version_str=$(bump_minor "$version_str")
else
  new_version_str=$(bump_patch "$version_str")
fi

if [ "$RELEASE" != "true" ]; then
  # Find the number all the commits between now and the last release.
  commits=$(git log HEAD..."$prev_tag" --pretty=format:"%H")

  # Find all file dependencies of the bazel target.
  bazel query 'kind("source file", deps('"$BAZEL_TARGET"'))' | sed  -e 's/:/\//' -e 's/^\/\+//' > target_files.txt
  trap "rm -f target_files.txt" EXIT

  commit_count=0
  # For each commit, check if it has modified a file in the bazel target's dependencies.
  for commit in $commits
  do
    files=$(git show --name-only --format=oneline "$commit" | tail -n +2)
    for file in $files
    do
      if grep -iq "$file" target_files.txt; then
        commit_count=$((commit_count + 1))
        break
      fi
    done
  done

  branch_name=$(git rev-parse --abbrev-ref HEAD)
  sanitized_branch=$(echo "$branch_name" | sed -E 's/[._\/]+/-/g')

  new_version_str=$(update_pre "$new_version_str" "$commit_count" "$sanitized_branch")
fi

changelog=$(generate_changelog "$prev_tag" "$BAZEL_TARGET")

new_tag="release/$ARTIFACT_TYPE/v"$new_version_str

git tag -a "$new_tag" -m "$changelog" --cleanup=whitespace

if [ "$PUSH" = "true" ]; then
  git push origin "$new_tag"
fi
