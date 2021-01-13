#!/bin/bash -e

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
    stirling_wrapper=$pixie_root/$(bazel_build //src/stirling:stirling_wrapper)
    go_grpc_server=$pixie_root/$(bazel_build //src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server:go_grpc_server)
    go_grpc_client=$pixie_root/$(bazel_build //src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client:go_grpc_client)
else
    # If the script was run through bazel, the locations are passed as arguments.
    stirling_wrapper=$1
    go_grpc_server=$2
    go_grpc_client=$3
fi

###############################################################################
# Test set-up
###############################################################################

# Run a GRPC client server as a uprobe target.
run_uprobe_target "$go_grpc_server" "$go_grpc_client"

echo "Test Setup complete."

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

flags="--init_only"
out=$(run_prompt_sudo "$stirling_wrapper" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

check_stirling_output "$out"
