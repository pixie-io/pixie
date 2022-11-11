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

workspace=$(git rev-parse --show-toplevel)
pushd "${workspace}" &> /dev/null || exit

function label_to_path() {
  path="${1#"//"}"
  echo "${path/://}"
}

function build() {
  # Exits with message if the bazel build command goes wrong.
  # Force bazel to download all targets since the output of
  # go_proto rules is a .a file and not the .pb.go which is
  # an intermediate output.
  if ! out=$(bazel build --remote_download_outputs=all "$@" 2>&1); then
    echo "${out}"
    exit 1
  fi
}

function copy() {
  for label in "$@"; do
    echo "Updating ${label} ..."

    path=$(label_to_path "${label}")
    dir=$(dirname "${path}")
    name=$(basename "${path}")
    # The omitted path component tolerates the host-dependent value by bazel's go rules.
    # Also the output pb.go would be identical between host OS, so there is no need to pick any
    # particular one.
    abs_path=$(find "bazel-bin/${dir}/${name}_" -name '*.pb.go' | head -n 1)
    if [[ "${abs_path}" == "" ]]; then
      echo "Failed to locate pb.go for ${label}"
      return 1
    fi
    cp -f "${abs_path}" "${dir}"
  done
}

if [[ $# == 0 ]]; then
  mapfile -t < <(bazel query --noshow_progress --noshow_loading_progress "kind('go_proto_library rule', //...)")
else
  MAPFILE=("$@")
fi

build "${MAPFILE[@]}"
copy "${MAPFILE[@]}"
