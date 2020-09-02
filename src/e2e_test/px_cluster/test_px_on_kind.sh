#!/bin/bash -eE

# This script creates a cluster on a Kind custer,
# then tries to deploy pixie and execute a simple pxl script.
# The pxl script that is run is one that requires BPF, so we can confirm
# BPF compatibility as well.

script_dir="$(dirname "$0")"

# shellcheck source=./src/e2e_test/px_cluster/common.sh
source "$script_dir"/common.sh

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
output=$(run_script)
check_results "$output"
