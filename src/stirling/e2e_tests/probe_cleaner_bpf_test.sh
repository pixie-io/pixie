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

# Note: This script will ask for root privileges.
#
# This script tests the probe_cleaner functions.
# It runs an instance of stirling_wrapper, and then does a SIGKILL to ensure it leaks probes.
# Then it runs the cleaner to clean those probes up.

if [ $# -ne 2 ]; then
  echo "Usage: $0 <stirling_wrapper> <probe_cleaner_bin>"
  echo "Example: probe_cleaner_bpf_test.sh bazel-bin/src/stirling/binaries/stirling_wrapper bazel-bin/src/stirling/bpf_tools/probe_cleaner_standalone"
  exit 1
fi

stirling_wrapper_bin=$1
probe_cleaner_bin=$2

# Switch to root user.
if [[ $EUID -ne 0 ]]; then
   sudo "$0" "$stirling_wrapper_bin" "$probe_cleaner_bin"
   exit
fi

# The marker we are looking for in identifying probes to kill.
marker="__pixie__"

function test_setup() {
    # Create a temporary file. Then open it, and remove the file.
    # This trick makes sure no garbage is left after the test exits.
    # The file is accessed in the rest of the script not by its name,
    # but rather by fd (e.g. `&3`).
    tmpfile=$(mktemp)
    exec 3> "$tmpfile" # FD for writing.
    exec 4< "$tmpfile" # FD for reading.
    rm "$tmpfile"

    $stirling_wrapper_bin 2>&3 > /dev/null &
    if [ $? -ne 0 ]; then
        echo "Test setup failed: Cannot run program"
        return 1
    fi
    pid=$!

    # Wait for the kprobes to deploy, by looking for the "Probes successfully deployed" message.
    tail -f -n +1 <&4 | sed '/Probes successfully deployed/ q'

    # This kill will cause the probes to leak.
    kill -KILL $pid
    wait $pid

    num_probes=$(grep -c $marker /sys/kernel/debug/tracing/kprobe_events)
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

    num_probes=$(grep -c $marker /sys/kernel/debug/tracing/kprobe_events)
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
