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

stirling_image=$1
go_grpc_server=$2
go_grpc_client=$3

###############################################################################
# Test set-up
###############################################################################

image_name=bazel/src/stirling/binaries:stirling_wrapper_image
podman load -i "$stirling_image"

echo "Running trace target."

# Run a GRPC client server as a uprobe target.
run_uprobe_target "$go_grpc_server" "$go_grpc_client"

###############################################################################
# Main test: Run stirling_wrapper container.
###############################################################################

echo "Running stirling_wrapper container."

flags="--timeout_secs=0"
out=$(podman run --rm \
 -v /:/host \
 -v /sys:/sys \
 --env PL_HOST_PATH=/host \
 --privileged \
 "$image_name" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "Checking results."

check_stirling_output "$out"
