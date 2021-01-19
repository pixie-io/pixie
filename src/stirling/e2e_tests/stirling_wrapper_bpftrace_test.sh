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
    stirling_wrapper=$pixie_root/$(bazel_build //src/stirling/binaries:stirling_wrapper)
    trace_script=$pixie_root/src/stirling/testing/tcpdrop.bpftrace.pxl
else
    # If the script was run through bazel, the locations are passed as arguments.
    stirling_wrapper=$1
    trace_script=$2
fi

###############################################################################
# Main test: Run stirling_wrapper.
###############################################################################

echo "Running stirling_wrapper"
flags="--timeout_secs=60 --trace=$trace_script"
# shellcheck disable=SC2086
out=$(run_prompt_sudo "$stirling_wrapper" $flags 2>&1)

###############################################################################
# Check output for errors/warnings.
###############################################################################

check_dynamic_trace_deployment() {
  out=$1
  echo "$out"

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

  record_count=$(echo "$out" | grep -c -e "^\[tcp_drop_table\]" || true)
  echo "Record count = $record_count"
  if [ "$record_count" -eq "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  echo "Test PASSED"
  return 0
}

check_dynamic_trace_deployment "$out"
