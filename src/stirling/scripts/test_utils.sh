#!/bin/bash -e

# This function deploys a GRPC client server as a target for uprobes.
# TODO(oazizi): go_grpc_server/go_grpc_client path hard-codes 'linux_amd64_debug'. Fix.
function run_uprobe_target() {
  server=$1
  client=$2

  # Stat so that set -e can error out if the files don't exist.
  stat "$server" > /dev/null
  stat "$client" > /dev/null

  # Run client-server.
  "$server" > server_out &
  sleep 1
  port=$(cat server_out)
  echo "Server port: $port"
  "$client" --address 127.0.0.1:"$port" &> client_out &
}

# This function checks the output of stirling in --init_mode for errors or warnings.
# Used by tests.
check_stirling_output() {
  out=$1
  echo "$out"

  # Look for GLOG errors or warnings, which start with E or W respectively.
  err_count=$(echo "$out" | grep -c -e ^E -e ^W || true)
  echo "Error/Warning count = $err_count"
  if [ "$err_count" != "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  # Look for number deployed probes
  num_kprobes=$(echo "$out" | sed -n "s/.*Number of kprobes deployed = //p")
  if [ "$num_kprobes" = "0" ]; then
    echo "No kprobes deployed"
    echo "Test FAILED"
    return 1
  fi

  # Look for number deployed probes
  num_uprobes=$(echo "$out" | sed -n "s/.*Number of uprobes deployed = //p")
  if [ "$num_uprobes" = "0" ]; then
    echo "No uprobes deployed"
    echo "Test FAILED"
    return 1
  fi

  success_msg=$(echo "$out" | grep -c -e "Probes successfully deployed" || true)
  if [ "$success_msg" != "1" ]; then
    echo "Could not find success message"
    echo "Test FAILED"
    return 1
  fi

  echo "Test PASSED"
  return 0
}
