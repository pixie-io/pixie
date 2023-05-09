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
set -o pipefail

diff_file=""
build=true

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-d] [-h] [-f file_name] [-n]" 1>&2;
  echo "   -f    Use a diff file"
  echo "   -n    Don't run the build"
  echo "   -h    Print help and exit"
  exit 1;
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "f:hn" opt; do
    case ${opt} in
      n)
        build=false
        ;;
      f)
        diff_file=$OPTARG
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

if [[ -n "${diff_file}" && ! -s "${diff_file}" ]]; then
  echo "Diff file is empty, exiting"
  echo "Diff file ${diff_file} empty" > clang_tidy.log
  exit 0
fi

clang_tidy_diff_scripts=(
  "/opt/px_dev/bin/clang-tidy-diff.py"
  "/usr/local/opt/llvm/share/clang/clang-tidy-diff.py"
)

clang_tidy_script=""
for script_option in "${clang_tidy_diff_scripts[@]}"; do
  echo "Looking for ${script_option}"
  if [ -f "${script_option}" ]; then
    clang_tidy_script=${script_option}
    break
  fi
done

if [ -z "${clang_tidy_script}" ]; then
  echo "Failed to find a valid clang tidy script runner (check LLVM/Clang install)"
  exit 1
fi

echo "Selected: ${clang_tidy_script}"

pushd "$(bazel info workspace)" &>/dev/null
echo "Generating compilation database..."

flags=("--include_headers")
if [ "$build" = true ] ; then
  flags+=("--run_bazel_build")
fi

# Bazel build need to be run to setup virtual includes, generating files which are consumed
# by clang-tidy.
./scripts/gen_compilation_database.py "${flags[@]}"

# Actually invoke clang-tidy.
if [ -z "$diff_file" ] ; then
  git diff -U0 origin/main -- src | "${clang_tidy_script}" -p1 2>&1 | tee clang_tidy.log
else
  "${clang_tidy_script}" -p1 2>&1 < "${diff_file}" | tee clang_tidy.log
fi

popd &>/dev/null
