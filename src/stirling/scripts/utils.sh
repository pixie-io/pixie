#!/bin/bash -e

# TODO(oazizi): Consider splitting into two files. One for general utils, one for stirling specific test utils.

# If not root, this command runs the provided command through sudo.
# Meant to be used interactively when not root, since sudo will prompt for password.
run_prompt_sudo() {
  cmd=$1
  shift

  if [[ $EUID -ne 0 ]]; then
     if [[ $(stat -c '%u' "$(command -v sudo)") -eq 0 ]]; then
        sudo "$cmd" "$@"
      else
        echo "ERROR: Cannot run as root"
      fi
  else
     "$cmd" "$@"
  fi
}

# This function builds the bazel target and returns the path to the output, relative to ToT.
function bazel_build() {
  target=$1
  bazel_out=$(bazel build "$target" 2>&1)
  echo "$bazel_out" | grep -A1 "^Target" | tail -1 | sed -e 's/^[[:space:]]*//'
}

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
  if [ "$err_count" -ne "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  # Look for number deployed probes
  num_kprobes=$(echo "$out" | sed -n "s/.*Number of kprobes deployed = //p")
  if [ "$num_kprobes" -eq "0" ]; then
    echo "No kprobes deployed"
    echo "Test FAILED"
    return 1
  fi

  # Look for number deployed probes
  num_uprobes=$(echo "$out" | sed -n "s/.*Number of uprobes deployed = //p")
  if [ "$num_uprobes" -eq "0" ]; then
    echo "No uprobes deployed"
    echo "Test FAILED"
    return 1
  fi

  success_msg=$(echo "$out" | grep -c -e "Probes successfully deployed" || true)
  if [ "$success_msg" -ne "1" ]; then
    echo "Could not find success message"
    echo "Test FAILED"
    return 1
  fi

  echo "Test PASSED"
  return 0
}
