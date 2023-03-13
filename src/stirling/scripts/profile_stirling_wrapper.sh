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

# This script profiles stirling using the linux perf tool.

###############################################################################
# Arguments
###############################################################################

script_dir=$(dirname "$0")

# shellcheck source=./src/stirling/scripts/utils.sh
source "$script_dir"/utils.sh

usage() {
  echo "This script profiles stirling using the linux perf tool."
  echo ""
  echo "Usage: $0 [-t=<runtime in seconds>] -- [<arguments to pass to stirling>]"
  echo ""
  echo "The time parameter controls the duration for which Stirling should be run and profiled."
  echo "The duration applies only after Stirling has initialized."
  exit
}

parse_args() {
  # Set defaults here.
  Tseconds=60
  outfile=perf-$(date '+%Y%m%d%H%M%S').data

  # Process the command line arguments.
  while getopts "t:o:h" opt; do
    case ${opt} in
      t)
        Tseconds=$OPTARG
        ;;
      o)
        outfile=$OPTARG
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

parse_args "$@"
shift $((OPTIND -1))

###############################################################################
# Build Stirling
###############################################################################

bazel_flags=(-c opt)

bazel build "${bazel_flags[@]}" //src/stirling/binaries:stirling_wrapper
cmd=$(bazel cquery "${bazel_flags[@]}" //src/stirling/binaries:stirling_wrapper --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)

###############################################################################
# Run Stirling
###############################################################################

# This funky syntax runs stirling in the background,
# while redirecting its output to file descriptor 3.
# This allows us to run stirling asynchronously, while still capturing its output.
exec 3< <(sudo "$cmd" --timeout_secs="$Tseconds" 2>&1)

# Extract the Stirling PID.
pid_line=$(grep -m 1 'Stirling Wrapper PID:' <&3)
pid=$(echo "$pid_line" | awk -F"PID: " '{print $2}' | awk -F" " '{print $1}')
echo "Stirling PID = $pid"

# Now wait to see that Stirling has passed its initialization phase.
grep -m 1 'Stirling is running.' <&1 &> /dev/null

###############################################################################
# Run Perf profiler
###############################################################################

# Finally, we can attach perf, such that it monitors Stirling post-init,
# so the perf numbers are not tainted with the init, which is intensive.
echo "Stirling has deployed with a runtime of $Tseconds seconds."
echo "Now attaching perf."
sudo perf record -o "$outfile" --call-graph lbr -p "$pid"

echo "Done collecting perf data. To view results, try running:"
echo "  sudo perf report -f -i $outfile"
