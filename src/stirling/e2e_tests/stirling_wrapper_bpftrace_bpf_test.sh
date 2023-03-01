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

# Clean-up any spawned background processes on exit.
trap 'kill $(jobs -p) &> /dev/null || true' SIGINT SIGTERM EXIT

# shellcheck source=./src/stirling/scripts/utils.sh
source src/stirling/scripts/utils.sh

# shellcheck source=./src/stirling/scripts/test_utils.sh
source src/stirling/scripts/test_utils.sh

stirling_wrapper=$1
trace_script=$2

###############################################################################
# Workload for test: This test needs binaries running in the background for
# stirling_wrapper to capture.
###############################################################################
workload() {
    for _ in {1..120}; do
	sleep 1
    done
}

workload &
workload_pid=$!

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

echo "Running stirling_wrapper"
flags=("--timeout_secs=60" "--trace=$trace_script")

stirling_wrapper_log=$(mktemp --suffix=".log")
stirling_wrapper_pid_file=$(mktemp --suffix=".pid")
already_killed=0
("$stirling_wrapper" "${flags[@]}" & echo $! > "${stirling_wrapper_pid_file}") 2>&1 | tee "${stirling_wrapper_log}" |\
    while read -r line; do
	echo "${line}"
	# Once we see any exec_snoop records we can stop stirling_wrapper to exit early.
	record_count=$(echo "${line}" | grep -c -e "^\[exec_snoop_table\]" || true)
	if [[ "${record_count}" -gt "0" && "${already_killed}" -eq "0" ]]; then
	    already_killed=1
	    stirling_wrapper_pid=$(cat "${stirling_wrapper_pid_file}")
	    kill "${stirling_wrapper_pid}" > /dev/null 2>&1 || true
	fi
    done

###############################################################################
# Check output for errors/warnings.
###############################################################################

check_dynamic_trace_deployment() {
  out=$1

  # Look for GLOG errors or warnings, which start with E or W respectively.
  err_count=$(echo "$out" | grep -c -e ^E -e ^W || true)
  echo "Error/Warning count = $err_count"
  if [ "$err_count" -ne "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  success_msg=$(echo "$out" | grep -c -e "Successfully deployed dynamic trace!" || true)
  if [ "$success_msg" -ne "1" ]; then
    echo "Could not find success message"
    echo "Test FAILED"
    return 1
  fi

  record_count=$(echo "$out" | grep -c -e "^\[exec_snoop_table\]" || true)
  echo "Record count = $record_count"
  if [ "$record_count" -eq "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  echo "Test PASSED"
  return 0
}

# We can stop the workload generator now that stirling_wrapper has quit.
kill "${workload_pid}" > /dev/null 2>&1 || true

check_dynamic_trace_deployment "$(cat "${stirling_wrapper_log}")"
