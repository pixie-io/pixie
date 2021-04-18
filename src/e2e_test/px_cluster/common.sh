#!/bin/bash

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

# Deploy pixie.
function px_deploy() {
  date
  px deploy -y
  date
  # Wait some additional time for pods to settle, just to be safe.
  sleep 30
}

function get_cluster_id() {
  cluster_name=$1
  px get viziers | grep " $cluster_name " | tr -s ' ' | cut -f3 -d' '
}

# Run a simple script. Could add more scripts to expand coverage.
function run_script() {
  cluster_id=$1
  px -c "$cluster_id" script run px/demo_script
}

function check_results() {
  output=$1

  echo "Sample output:" >&2
  echo "$output" | head -10 >&2

  num_rows=$(echo "$output" | wc -l)
  # There are two header lines, so look for at least 3 lines.
  if [ "$num_rows" -lt 3 ]; then
    echo "Test FAILED: Not enough results"
    return 1
  fi

  echo "Test PASSED"
  return 0
}
