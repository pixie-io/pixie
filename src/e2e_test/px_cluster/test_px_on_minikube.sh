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

# This script creates a cluster on a Minikube custer,
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
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <minikube-binary> <container-runtime>"
  echo "e.g. $0 /usr/local/bin/minikube docker"
  echo "e.g. $0 minikubes/v1.15.0/minikube containerd"
  exit 1
fi

minikube_bin=$1
runtime=$2

if [ "$minikube_bin" == "minikube" ]; then
  minikube_bin=$(command -v minikube)
fi

echo "Using minikube: $minikube_bin"
echo "Using container runtime: $runtime"

if [ ! -f "$minikube_bin" ]; then
  echo "Could not find minikube binary: $minikube_bin"
  exit 1
fi

driver_name="kvm2"
if [ "$(uname)" == "Darwin" ]; then
  # for mac:
  driver_name="hyperkit"
fi

# Create a random cluster name.
cluster_name="test-cluster-${RANDOM}"

# Create the cluster
$minikube_bin start --driver="$driver_name" --container-runtime="$runtime" -p "$cluster_name"

function  cleanup() {
  $minikube_bin stop -p "$cluster_name"
  $minikube_bin delete -p "$cluster_name"
}

# Delete cluster on exit (covers error cases too).
trap cleanup EXIT

# Test pixie here.
px_deploy
cluster_id=$(get_cluster_id "$cluster_name")
output=$(run_script "$cluster_id")

# cleanup & check results
check_results "$output"
