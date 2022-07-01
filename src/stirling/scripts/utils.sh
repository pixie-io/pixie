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

# If not root, this command runs the provided command through sudo.
# Meant to be used interactively when not root, since sudo will prompt for password.
run_prompt_sudo() {
  cmd=$1
  shift

  if [[ $EUID -ne 0 ]]; then
    if [[ $(stat -c '%u' "$(command -v sudo)") -eq 0 ]]; then
      sudo "$cmd" "$@"
    else
      echo "ERROR: Cannot run as root"
    fi
  else
    "$cmd" "$@"
  fi
}

# This function builds the bazel target and returns the path to the output, relative to ToT.
function bazel_build() {
  target=$1
  flags=${2:-""}
  # shellcheck disable=SC2086
  bazel_out=$(bazel build $flags "$target" 2>&1)
  retval=$?

  # If compile failed, log and abort.
  if [ $retval -ne 0 ]; then
    echo "$bazel_out" > /tmp/bazel_build.out
    # Echo to stderr, so it gets printed even when invoked inside a sub-shell.
    >&2 echo "Failed to build stirling_wrapper. See logs in /tmp/bazel_build.out."
    return $retval
  fi

  # This funky command looks for and parses out the binary from a output like the following:
  # ...
  # Target //src/stirling/binaries:stirling_wrapper up-to-date:
  #   bazel-bin/src/stirling/binaries/stirling_wrapper
  # ...
  binary=$(echo "$bazel_out" | grep -A1 "^Target .* up-to-date" | tail -1 | sed -e 's/^[[:space:]]*//')
  echo "$binary"
}

function docker_stop() {
  container_name=$1
  echo "Stopping container $container_name ..."
  docker container stop "$container_name"
}

function docker_load() {
  image=$1
  docker load -i "$image" | awk '/Loaded image/ {print $NF}'
}
