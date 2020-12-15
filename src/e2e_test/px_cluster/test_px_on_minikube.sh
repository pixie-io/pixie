#!/bin/bash -eE

# This script creates a cluster on a Minikube custer,
# then tries to deploy pixie and execute a simple pxl script.
# The pxl script that is run is one that requires BPF, so we can confirm
# BPF compatibility as well.

function  cleanup() {
  $minikube_bin stop -p "$cluster_name"
  $minikube_bin delete -p "$cluster_name"
}

script_dir="$(dirname "$0")"

# shellcheck source=./src/e2e_test/px_cluster/common.sh
source "$script_dir"/common.sh

# Get arguments
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <minikube-binary>"
  echo "e.g. $0 /usr/local/bin/minikube"
  echo "e.g. $0 minikubes/v1.15.0/minikube"
  exit 1
fi

minikube_bin=$1

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
$minikube_bin start --driver="$driver_name" -p "$cluster_name"

# Delete cluster on exit (covers error cases too).
trap cleanup EXIT

# Test pixie here.
px_deploy
cluster_id=$(get_cluster_id "$cluster_name")
output=$(run_script "$cluster_id")

# cleanup & check results
check_results "$output"
