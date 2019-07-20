#!/bin/bash

# NOTE: This test must currently be run manually, and as root.
# If you run it as a regular user, it will use sudo and prompt for sudo access.
#
# This test checks that Stirling cleans up after itself after it receives
# signals such as SIGTERM and SIGINT. In particular, it is important that
# Stirling not leave any attached kprobes, as these won't be cleaned up by
# the kernel.
#
# Note: the following command can be used to remove all kprobes, if needed.
#    sudo sh -c 'echo > /sys/kernel/debug/tracing/kprobe_events'
#
# Perf buffers and perf events are assigned file descriptors,
# so hopefully the kernel gets rid of them when the process dies, right?

if [ $# -eq 0 ]; then
  echo "Usage: $0 <command to test for leaks>"
  echo "Example: ./kprobe_leak_test.sh \$(bazel info bazel-bin)/src/stirling/stirling_wrapper"
  exit 1
fi

test_cmd=$1

# Switch to root user.
if [[ $EUID -ne 0 ]]; then
   sudo $0 $test_cmd
   exit
fi

echo "Program to test: $test_cmd"

if [ -z "$test_cmd" ]; then
  echo "Error: no command to test"
  exit 1
fi

function test() {
    signal=$1

    echo "---------------"
    echo "Testing $signal"

    num_probes_orig=$(cat /sys/kernel/debug/tracing/kprobe_events | wc -l)
    echo "Initial number of probes: $num_probes_orig"

    $test_cmd > /dev/null &
    if [ $? -ne 0 ]; then
        echo "FAILED: Cannot run program"
        return 1
    fi
    pid=$!
    echo "Program PID: $pid"

    # Delayed kill
    sh -c "sleep 3 && kill -$signal $pid" &

    # Wait for process to terminate.
    wait $pid

    num_probes_final=$(cat /sys/kernel/debug/tracing/kprobe_events | wc -l)
    echo "Final number of probes: $num_probes_final"

    if [ $num_probes_orig -ne $num_probes_final ]; then
      echo "Test FAILED: Program is leaking BPF probes"
      return 1
    fi

    echo "Test PASSED"
    return 0
}

errors=0

test TERM
errors=$((errors+$?))

test HUP
errors=$((errors+$?))

test INT
errors=$((errors+$?))

test QUIT
errors=$((errors+$?))

echo "--------------"
if [ $errors -ne 0 ]; then
    echo "There were $errors FAILED tests"
else
    echo "All tests PASSED"
fi

exit $errors
