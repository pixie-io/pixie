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

usage() {
  echo "This script effectively executes 'bazel run' with sudo"
  echo ""
  echo "Usage: $0 <bazel_flags> <bazel_target> [-- <arguments to binary>] [-- <pass thru env vars>]"
  echo ""
  echo "Example:"
  echo "  # To run the following as root (contrived to show bazel args, test args, and pass thru env. vars):"
  echo "  # PL_HOST_PATH=/tmp/pl bazel run -c dbg //src/stirling:socket_trace_bpf_test -- --gtest_filter=Test.Test"
  echo "  PL_HOST_PATH=/tmp/pl scripts/sudo_bazel_run.sh -c dbg //src/stirling:socket_trace_bpf_test -- --gtest_filter=Test.Test -- PL_HOST_PATH"

  exit
}

if [ $# -eq 0 ]; then
  usage
fi

if [ "$1" == "-h" ]; then
  usage
fi

# Assuming script is installed in $PIXIE_ROOT/bin
script_dir="$(dirname "$0")"
cd "$script_dir"/.. || exit

# Extract bazel build args and target.
# But leave runtime arguments in the array.
for x in "$@"; do
  # Look for the argument separator.
  if [[ "$x" == "--" ]]; then
    shift
    break;
  fi
  build_args+=("$x")
  shift
done

# Extract run args.
# But leave pass through env. vars in the array.
for x in "$@"; do
  # Look for the argument separator.
  if [[ "$x" == "--" ]]; then
    shift
    break;
  fi
  run_args+=("$x")
  shift
done

for x in "$@"; do
  # Fail if an additional argument separator is found.
  if [[ "$x" == "--" ]]; then
    echo "Extra \"--\" indicates more args, but this script does not understand them."
    exit 1;
  fi
  # ${!x} expands to the value assigned to the env. var whose name is stored in x.
  # If $x expands to FOO, and FOO is an env. var with value "123" then ${!x} expands to 123.
  pass_thru_env_args+=("$x=${!x}")
  pass_thru_env_vars+=("$x")
  shift
done

options=("${build_args[@]::${#build_args[@]}-1}")
target="${build_args[-1]}"

echo "Bazel options: ${options[*]}"
echo "Bazel target: $target"
echo "Run args: ${run_args[*]}"
echo "Pass through env. vars: ${pass_thru_env_vars[*]}"

# Perform the build as user (not as root).
bazel build "${options[@]}" "$target"

# Now find the bazel-bin directory.
bazel_bin=$(bazel info "${options[@]}" bazel-bin 2> /dev/null)

# Modify the target from //dir/subdir:exe to ${bazel_bin}/dir/subdir/exe.
target=${target//":"/"/"}
target=${target//"//"/""}
target=${bazel_bin}/${target}

# Run the binary with sudo.
sudo "${pass_thru_env_args[@]}" "$target" "${run_args[@]}"
