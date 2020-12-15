#!/bin/bash

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
