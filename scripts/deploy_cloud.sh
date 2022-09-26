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

# This deploys a new cloud release.

usage() {
    echo "Usage: echo <changelog message> | $0 [-r]"
    echo " -r : Create a prod release."
    echo "Example: echo 'this is a cloud prod release' | $0 -r"
    exit 1
}

parse_args() {
  local OPTIND
  while getopts "r" opt; do
    case ${opt} in
      r)
        RELEASE=true
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND - 1))
}

parse_args "$@"

# Get input from stdin.
CHANGELOG=''
while IFS= read -r line; do
    CHANGELOG="${CHANGELOG}${line}\n"
done

set -x
timestamp="$(date +"%s")"
release="staging"
if [ "$RELEASE" = "true" ]; then
  release="prod"
fi

new_tag="release/cloud/$release/$timestamp"
git tag -a "$new_tag" -m "$(echo -e "$CHANGELOG")"

git push origin "$new_tag"
