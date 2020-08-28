#!/bin/bash
set -e

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

# First Get arguments
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <image type=UBUNTU|UBUNTU_CONTAINERD|COS|COS_CONTAINERD>"
  exit 1
fi

image_type="$1"

# Create a random cluster name.
cluster_name="test-cluster-${RANDOM}"

# Create the cluster
./create_gke_cluster.sh -c "$cluster_name" -n 3 -i "$image_type"

# Deploy pixie.
px deploy -q
sleep 60

# Run a simple script. Could add more scripts to expand coverage.
output=$(px script run px/http_data)
num_rows=$(echo "$output" | wc -l)
echo "Sample output:"
echo "$output" | head -10

# Wrap up by deleting the cluster.
gcloud container clusters delete "$cluster_name" --quiet

# Check results after destroying the cluster, to make sure we always clean-up.

# There are two header lines, so look for at least 3 lines.
if [ "$num_rows" -lt 3 ]; then
  echo "Test FAILED: Not enough results"
  return 1
fi
echo "Test PASSED"
