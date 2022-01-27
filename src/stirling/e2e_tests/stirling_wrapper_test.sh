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

# shellcheck source=./src/stirling/scripts/test_utils.sh
source "$pixie_root"/src/stirling/scripts/test_utils.sh

if [ -z "$BUILD_WORKSPACE_DIRECTORY" ] && [ -z "$TEST_TMPDIR" ]; then
    # If the script was run in a stand-alone way, then build and set paths.
    echo "Running test in stand-alone mode"
    echo "Building stirling_wrapper and test binaries. This may take some time."
    stirling_wrapper=$pixie_root/$(bazel_build //src/stirling/binaries:stirling_wrapper)
    go_grpc_server=$pixie_root/$(bazel_build //src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server:go_1_16_grpc_server)
    go_grpc_client=$pixie_root/$(bazel_build //src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client:go_1_16_grpc_client)
else
    # If the script was run through bazel, the locations are passed as arguments.
    stirling_wrapper=$1
    go_grpc_server=$2
    go_grpc_client=$3
fi

###############################################################################
# Test set-up
###############################################################################

echo "Running trace target."

# Run a GRPC client server as a uprobe target.
run_uprobe_target "$go_grpc_server" "$go_grpc_client"

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

echo "Running stirling_wrapper."

flags="--color_output=false --timeout_secs=0"
out=$(run_prompt_sudo "$stirling_wrapper" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "Checking results."

check_stirling_output "$out"
