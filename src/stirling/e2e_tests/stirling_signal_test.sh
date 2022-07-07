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

# TODO(PP-3021): This test shows flakiness. Find and fix the root cause.
# TODO(PP-3022): This test cannot be run with scripts/sudo_bazel_run.sh. Find and fix the root
# cause.

# Clean-up any spawned background processes on exit.
trap 'kill $(jobs -p) &> /dev/null || true' SIGINT SIGTERM EXIT

# shellcheck source=./src/stirling/scripts/utils.sh
source src/stirling/scripts/utils.sh

stirling_wrapper=$1
stirling_ctrl=$2

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

# Create a temporary file. Then open it, and remove the file.
# This trick makes sure no garbage is left after the test exits.
# The file is accessed in the rest of the script not by its name,
# but rather by fd (e.g. `&3`).
tmpfile=$(mktemp)
exec 3> "$tmpfile" # FD for writing.
exec 4< "$tmpfile" # FD for reading.
rm "$tmpfile"

echo "Running stirling_wrapper."

# Disable all sources, as signal handling has nothing to do with them.
# This makes test run much faster. Also allows running without root permission.
"$stirling_wrapper" --stirling_sources= 2>&3 &

out=$(tail -f -n +1 <&4 | sed '/Stirling Wrapper PID/ q')
echo "$out"

# shellcheck disable=SC2086
stirling_pid=$(echo $out | awk '{print $NF}')
echo "Found PID = $stirling_pid"

echo "Sleep 3 seconds for stirling_wrapper to be ready ..."
sleep 3

echo "[Debug] Set global debug level to 1"
sleep 1 && "$stirling_ctrl" "$stirling_pid" 1 1
echo "[Debug] Set global debug level to 0"
sleep 1 && "$stirling_ctrl" "$stirling_pid" 1 0
echo "[Debug] Enable tracing on PID 12345"
sleep 1 && "$stirling_ctrl" "$stirling_pid" 2 12345
echo "[Debug] Disable tracing on PID 12345"
sleep 1 && "$stirling_ctrl" "$stirling_pid" 2 -12345
echo "[Debug] Kill stirling"
sleep 1 && kill "$stirling_pid"

out=$(cat <&4)
echo "$out"

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "Checking results."

if ! grep -q "Setting debug level to 1" <<< "$out"; then
  echo "FAILED: Setting debug level to 1"
  echo "Test FAILED (1)"
  exit 1
fi

if ! grep -q "Setting debug level to 0" <<< "$out"; then
  echo "FAILED: Setting debug level to 0"
  echo "Test FAILED (2)"
  exit 1
fi

if ! grep -q "Enabling tracing of PID: 12345" <<< "$out"; then
  echo "FAILED: Enabling tracing of PID: 12345"
  echo "Test FAILED (3)"
  exit 1
fi

if ! grep -q "Disabling tracing of PID: 12345" <<< "$out"; then
  echo "FAILED: Disabling tracing of PID: 12345"
  echo "Test FAILED (4)"
  exit 1
fi

echo "Test PASSED"
exit 0
