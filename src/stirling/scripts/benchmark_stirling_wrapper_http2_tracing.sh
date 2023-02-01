#!/bin/bash

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

# Runs stirling_wrapper to trace high-qps gRPC messages between a simple gRPC client & server.
# Then looking for any missing, malformed records.
# This must be run from the top of bazel repo.

BAZEL_BUILD_CMD="bazel build --compilation_mode=opt"

TEST_EXE_BASE_DIR="bazel-bin/src/stirling/source_connectors/socket_tracer/protocols/http2/testing"

GRPC_CLIENT_LABEL="src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client:golang_1_17_grpc_client"
GRPC_CLIENT_EXE="${TEST_EXE_BASE_DIR}/go_grpc_client/golang_1_17_grpc_client"

GRPC_SERVER_LABEL="src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server:golang_1_17_grpc_server"
GRPC_SERVER_EXE="${TEST_EXE_BASE_DIR}/go_grpc_server/golang_1_17_grpc_server"

STIRLING_WRAPPER_LABEL="src/stirling/binaries:stirling_wrapper"
STIRLING_WRAPPER_EXE="bazel-bin/src/stirling/binaries/stirling_wrapper"

${BAZEL_BUILD_CMD} ${GRPC_CLIENT_LABEL}
${BAZEL_BUILD_CMD} ${GRPC_SERVER_LABEL}
${BAZEL_BUILD_CMD} ${STIRLING_WRAPPER_LABEL}

echo "Building finished, to benchmark HTTP2 tracing:"
echo "In one shell window, launch gRPC server:"
echo "${GRPC_SERVER_EXE}"
echo "In another shell window, launch stirling_wrapper:"
echo "sudo ${STIRLING_WRAPPER_EXE} --sources=socket_tracer --print_record_batches=http_events"
echo "In yet another shell window, launch gRPC client:"
echo "${GRPC_CLIENT_EXE} --count=100000 --wait_period_millis=0"
