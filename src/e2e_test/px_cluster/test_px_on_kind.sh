#!/bin/bash -eE

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

# shellcheck disable=SC1090

# This script creates a cluster on a Kind custer,
# then tries to deploy pixie and execute a simple pxl script.
# The pxl script that is run is one that requires BPF, so we can confirm
# BPF compatibility as well.

# --- begin runfiles.bash initialization v2 ---
# Copy-pasted from the Bazel Bash runfiles library v2.
set -uo pipefail; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v2 ---

# shellcheck source=./src/e2e_test/px_cluster/common.sh
source "$(rlocation px/src/e2e_test/px_cluster/common.sh)"

# Get arguments
if [ "$#" -ne 0 ]; then
  echo "Usage: $0"
  exit 1
fi

# Create a random cluster name.
cluster_name="test-cluster-${RANDOM}"

# Create the cluster
kind create cluster --name "$cluster_name"

# Delete cluster on exit (covers error cases too).
trap 'kind delete cluster --name "$cluster_name"' EXIT

# Test pixie here.
px_deploy
cluster_id=$(get_cluster_id "$cluster_name")
output=$(run_script "$cluster_id")
check_results "$output"
