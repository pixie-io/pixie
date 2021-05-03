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

script_dir="$(dirname "$0")"
pixie_root="$script_dir"/../../..

# shellcheck source=./src/stirling/scripts/utils.sh
source "$pixie_root"/src/stirling/scripts/utils.sh

if [ -z "$BUILD_WORKSPACE_DIRECTORY" ] && [ -z "$TEST_TMPDIR" ]; then
    # If the script was run in a stand-alone way, then build and set paths.
    stirling_wrapper=$pixie_root/$(bazel_build //src/stirling/binaries:stirling_wrapper)
    stirling_wrapper=$pixie_root/$(bazel_build //src/stirling/binaries:stirling_ctrl)
else
    # If the script was run through bazel, the locations are passed as arguments.
    stirling_wrapper=$1
    stirling_ctrl=$2
fi

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

echo "Running stirling_wrapper."

# Create a temporary file. Then open it, and remove the file.
# This trick makes sure no garbage is left after the test exits.
# The file is accessed in the rest of the script not by its name,
# but rather by fd (e.g. `&3`).
tmpfile=$(mktemp)
exec 3> "$tmpfile" # FD for writing.
exec 4< "$tmpfile" # FD for reading.
rm "$tmpfile"

run_prompt_sudo "$stirling_wrapper" 2>&3 &

out=$(tail -f -n +1 <&4 | sed '/Stirling Wrapper PID/ q')

echo "$out"
# shellcheck disable=SC2086
stirling_pid=$(echo $out | awk '{print $NF}')
echo "Found PID = $stirling_pid"

out=$(tail -f -n +1 <&4 | sed '/Stirling successfully initialized./ q')

sleep 1 && run_prompt_sudo "$stirling_ctrl" "$stirling_pid" 1 1
sleep 1 && run_prompt_sudo "$stirling_ctrl" "$stirling_pid" 1 0
sleep 1 && run_prompt_sudo "$stirling_ctrl" "$stirling_pid" 2 12345
sleep 1 && run_prompt_sudo "$stirling_ctrl" "$stirling_pid" 2 -12345
sleep 1 && run_prompt_sudo kill "$stirling_pid"

out=$(cat <&4)
echo "$out"

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "Checking results."

if ! grep -q "Setting debug level to 1" <<< "$out"; then
  echo "Test FAILED (1)"
  exit 1
fi

if ! grep -q "Setting debug level to 0" <<< "$out"; then
  echo "Test FAILED (2)"
  exit 1
fi

if ! grep -q "Enabling tracing of PID: 12345" <<< "$out"; then
  echo "Test FAILED (3)"
  exit 1
fi

if ! grep -q "Disabling tracing of PID: 12345" <<< "$out"; then
  echo "Test FAILED (4)"
  exit 1
fi

echo "Test PASSED"
exit 0
