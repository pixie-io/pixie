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

# This test checks the performance overhead of stirling_wrapper.
# Two ways to run this test:
# 1) Locally. It will build in -c opt mode and prompt for password to run with sudo.
#   ./src/stirling/stirling_perf_test.sh
# 2) Through bazel.
#    sudo echo # To prime sudo password, so it doesn't prompt again.
#    bazel run -c opt //src/stirling:stirling_perf_test
# Note that Jenkins uses the bazel approach, but doesn't need the sudo prompt,
# because it already has privileges.


# Clean-up any spawned background processes on exit.
trap 'kill $(jobs -p) &> /dev/null || true' SIGINT SIGTERM EXIT

# shellcheck source=./src/stirling/scripts/utils.sh
source src/stirling/scripts/utils.sh

# shellcheck source=./src/stirling/scripts/test_utils.sh
source src/stirling/scripts/test_utils.sh

stirling_wrapper=$1

# If not running with admin privileges, check if sudo will work.
if [[ $EUID -ne 0 ]]; then
  echo "If trying to run this locally, use: sudo ./src/stirling/stirling_perf_test.sh"
  echo "(or run 'sudo echo', enter password and try again)"

  # Check that sudo won't block, otherwise this will error and the script will exit.
  sudo -n echo
fi

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

TIMEOUT_SECS=60
echo "Running stirling_wrapper for $TIMEOUT_SECS seconds after init."
flags="--timeout_secs=$TIMEOUT_SECS --stirling_sources=kProd --print_record_batches="
out=$(run_prompt_sudo "$stirling_wrapper" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "$out"

# Grab the CPU usage line, then use awk to perform splits to get total CPU usage.
total_cpu_usage=$(echo "$out" | grep "CPU usage" | awk -F"CPU usage: " '{print $2}' \
                                                 | awk -F", " '{print $3}' \
                                                 | awk -F "%" '{print $1}')
echo "$total_cpu_usage"

CPU_USAGE_PCT_LIMIT=10

# Using awk because bash doesn't support floating point compare.
if (( $(awk -v a="$total_cpu_usage" -v b="$CPU_USAGE_PCT_LIMIT" 'BEGIN{print(a>b)}') )); then
  echo "Test FAILED"
  exit 1
fi

echo "Test PASSED"
exit 0
