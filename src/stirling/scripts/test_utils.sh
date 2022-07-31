#!/bin/bash -e

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
    echo "[FAILED]"
    return 1
  fi

  # Look for number deployed probes
  num_kprobes=$(echo "$out" | sed -n "s/.*Number of kprobes deployed = //p")
  if [ "$num_kprobes" = "0" ]; then
    echo "No kprobes deployed"
    echo "[FAILED]"
    return 1
  fi

  # Look for number deployed probes
  num_uprobes=$(echo "$out" | sed -n "s/.*Number of uprobes deployed = //p")
  if [ "$num_uprobes" = "0" ]; then
    echo "No uprobes deployed"
    echo "[FAILED]"
    return 1
  fi

  success_msg=$(echo "$out" | grep -c -e "Probes successfully deployed" || true)
  if [ "$success_msg" != "1" ]; then
    echo "Could not find success message"
    echo "[FAILED]"
    return 1
  fi

  echo "[OK]"
  return 0
}
