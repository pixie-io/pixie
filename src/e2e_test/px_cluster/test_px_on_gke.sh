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

# This script creates a cluster with the specified type of node,
# then tries to deploy pixie and execute a simple pxl script.
# The pxl script that is run is one that requires BPF, so we can confirm
# BPF compatibility as well.

# Node types should be specified using GKE types.
# The ones currently of interest to us include:
#   UBUNTU - Ubuntu node with docker as the container runtime.
#   UBUNTU_CONTAINERD - Ubuntu with containerd as the container runtime.
#   COS - Container-optimized OS with docker as the container runtime.
#   COS_CONTAINERD - Container-optimized OS with containerd as the container runtime.

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
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <image type=UBUNTU|UBUNTU_CONTAINERD|COS|COS_CONTAINERD>"
  exit 1
fi

image_type="$1"

# Create a random cluster name.
cluster_name="test-cluster-${RANDOM}"

# Create the cluster
gcloud beta container --project "pl-pixies" clusters create "${cluster_name}" \
  --zone "us-west1-a" \
  --username "admin" \
  --machine-type "e2-standard-4" \
  --image-type "${image_type}" \
  --disk-type "pd-ssd" \
  --disk-size 100 \
  --cluster-ipv4-cidr=/21 \
  --services-ipv4-cidr=/20 \
  --scopes "https://www.googleapis.com/auth/compute,https://www.googleapis.com/auth/devstorage.read_only,https://www.googleapis.com/auth/logging.write,https://www.googleapis.com/auth/monitoring.write,https://www.googleapis.com/auth/service.management,https://www.googleapis.com/auth/servicecontrol" \
  --num-nodes 3 \
  --enable-ip-alias \
  --network "projects/pl-pixies/global/networks/dev" \
  --subnetwork "projects/pl-pixies/regions/us-west1/subnetworks/us-west1-0" \
  --addons HorizontalPodAutoscaling,HttpLoadBalancing \
  --no-enable-autoupgrade \
  --no-enable-autorepair \
  --labels k8s-dev-cluster=\
  --security-group="gke-security-groups@pixielabs.ai" \
  --no-enable-stackdriver-kubernetes

# Delete cluster on exit (covers error cases too).
trap 'gcloud container clusters delete "$cluster_name" --quiet' EXIT

# Test pixie here.
px_deploy
cluster_id=$(get_cluster_id "$cluster_name")
output=$(run_script "$cluster_id")
check_results "$output"
