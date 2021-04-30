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

retry() {
  cmd=$1
  try=${2:-15}       # 15 by default
  sleep_time=${3:-3} # 3 seconds by default

  # Show help if a command to retry is not specified.
  [ -z "$1" ] && echo 'Usage: retry cmd [try=15 sleep_time=3]' && return 1

  # The unsuccessful recursion termination condition (if no retries left)
  [ $try -lt 1 ] && echo 'All retries failed.' && return 1

  # The successful recursion termination condition (if the function succeeded)
  $cmd && return 0

  echo "Execution of '$cmd' failed."

  # Inform that all is not lost if at least one more retry is available.
  # $attempts include current try, so tries left is $attempts-1.
  if [ $((try-1)) -gt 0 ]; then
    echo "There are still $((try-1)) retrie(s) left."
    echo "Waiting for $sleep_time seconds..." && sleep $sleep_time
  fi

  # Recurse
  retry $cmd $((try-1)) $sleep_time
}

ensure_namespace() {
  ns=$1
  # Wrapping as such avoids sending an error code in case ns is not found. Useful for scripts that set -e.
  ret=0
  kubectl get namespaces "${ns}" > /dev/null 2> /dev/null || ret=$?
  if [[ ret -ne 0 ]]; then
    kubectl create namespace ${ns}
  fi
}
