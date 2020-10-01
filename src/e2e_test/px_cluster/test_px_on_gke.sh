#!/bin/bash -eE

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

script_dir="$(dirname "$0")"

# shellcheck source=./src/e2e_test/px_cluster/common.sh
source "$script_dir"/common.sh

# Get arguments
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <image type=UBUNTU|UBUNTU_CONTAINERD|COS|COS_CONTAINERD>"
  exit 1
fi

image_type="$1"

# Create a random cluster name.
cluster_name="test-cluster-${RANDOM}"

# Create the cluster
"$script_dir"/../../../scripts/create_gke_cluster.sh -c "$cluster_name" -n 3 -i "$image_type"

# Delete cluster on exit (covers error cases too).
trap 'gcloud container clusters delete "$cluster_name" --quiet' EXIT

# Test pixie here.
px_deploy
cluster_id=$(get_cluster_id "$cluster_name")
output=$(run_script "$cluster_id")
check_results "$output"
