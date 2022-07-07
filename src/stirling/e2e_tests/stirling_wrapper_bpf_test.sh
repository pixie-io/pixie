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
go_grpc_server=$2
go_grpc_client=$3
pass_thru="${*:4}"

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
# shellcheck disable=SC2086
out=$(run_prompt_sudo "${stirling_wrapper}" $flags $pass_thru 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

echo "Checking results."

check_stirling_output "$out"
