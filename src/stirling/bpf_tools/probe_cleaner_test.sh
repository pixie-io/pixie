#!/bin/bash

# Note: This script will ask for root privileges.
#
# This script tests the probe_cleaner functions.
# It runs an instance of stirling_wrapper, and then does a SIGKILL to ensure it leaks probes.
# Then it runs the cleaner to clean those probes up.

if [ -z "$1" ]; then
    stirling_wrapper_bin=$(bazel info bazel-bin)/src/stirling/binaries/stirling_wrapper
    probe_cleaner_bin=$(bazel info bazel-bin)/src/stirling/utils/probe_cleaner_standalone
else
    stirling_wrapper_bin=$1
    probe_cleaner_bin=$2
fi

# Switch to root user.
if [[ $EUID -ne 0 ]]; then
   sudo "$0" "$stirling_wrapper_bin" "$probe_cleaner_bin"
   exit
fi

# Amount of time we give the executable to start up and deploy kprobes.
# TODO(oazizi): Switch to looking for the "Probes successfully deployed" message.
Tsleep=10

# The marker we are looking for in identifying probes to kill.
marker="__pixie__"

function test_setup() {
    $stirling_wrapper_bin > /dev/null &
    if [ $? -ne 0 ]; then
        echo "Test setup failed: Cannot run program"
        return 1
    fi
    pid=$!

    # Delayed kill
    sh -c "sleep $Tsleep && kill -KILL $pid" &

    # Wait for process to terminate.
    wait $pid

    num_probes=$(cat /sys/kernel/debug/tracing/kprobe_events | grep $marker | wc -l)
    echo "Number of leaked probes: $num_probes"

    if [ "$num_probes" -eq 0 ]; then
      echo "Test setup failed: Expected to leave leaked probes."
      return 1
    fi
}

function test() {
    $probe_cleaner_bin
    if [ $? -ne 0 ]; then
        echo "FAILED: Cannot run cleaner"
        return 1
    fi

    num_probes=$(cat /sys/kernel/debug/tracing/kprobe_events | grep $marker | wc -l)
    echo "Number of leaked probes: $num_probes"

    if [ "$num_probes" -ne 0 ]; then
      echo "Test FAILED: Found $num_probes that were not cleaned up."
      return 1
    else
      echo "Test PASSED"
      return 0
    fi
}

test_setup
if [ $? -ne 0 ]; then
    echo "Test setup failed...aborting"
    exit 1
fi

test
