#!/bin/bash -ex

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

# This deploys a new cloud release.

usage() {
    echo "Usage: echo <changelog message> | $0 <artifact_type> [-r]"
    echo " -r : Create a prod release."
    echo "Example: echo 'this is a cloud prod release' | $0 cloud -r"
}

parse_args() {
  if [ $# -lt 1 ]; then
    usage
  fi

  ARTIFACT_TYPE=$1
  shift

  while test $# -gt 0; do
      case "$1" in
        -r) RELEASE=true
            shift
            ;;
        *)  usage ;;
      esac
  done
}

check_args() {
    if [ "$ARTIFACT_TYPE" != "cloud" ] && [ "$ARTIFACT_TYPE" != "docs" ]; then
        echo "Unsupported artifact type."
        exit
    fi
}

parse_args "$@"
check_args

# Get input from stdin.
CHANGELOG=''
while IFS= read -r line; do
    CHANGELOG="${CHANGELOG}${line}\n"
done

timestamp="$(date +"%s")"
release="staging"
if [ "$RELEASE" = "true" ]; then
  release="prod"
fi

new_tag="release/$ARTIFACT_TYPE/$release/$timestamp"
git tag -a "$new_tag" -m "$(echo -e "$CHANGELOG")"

git push origin "$new_tag"
