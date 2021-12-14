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

diff_mode=false
diff_file=""
build=true

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-d] [-h] [-f file_name] [-n]" 1>&2;
  echo "   -d    Run only diff against main branch"
  echo "   -f    Use a diff file"
  echo "   -n    Don't run the build"
  echo "   -h    Print help and exit"
  exit 1;
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "df:hn" opt; do
    case ${opt} in
      d)
        diff_mode=true
        ;;
      n)
        build=false
        ;;
      f)
        diff_mode=true
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

if [[ "${diff_mode}" = true && ! -z "${diff_file}" && ! -s "${diff_file}" ]]; then
    echo "Diff file is empty, exiting"
    echo "Diff file ${diff_file} empty" > clang_tidy.log
    exit 0
fi


# We can have the Clang tidy script in a few different places. Check them in priority
# order.
clang_tidy_full_scripts=(
    "/opt/clang-13.0/share/clang/run-clang-tidy.py"
    "/usr/local/opt/llvm/share/clang/run-clang-tidy.py")

clang_tidy_diff_scripts=(
    "/opt/clang-13.0/share/clang/clang-tidy-diff.py"
    "/usr/local/opt/llvm/share/clang/clang-tidy-diff.py")

search_scripts="${clang_tidy_full_scripts[@]}"
if [ "$diff_mode" = true ] ; then
  search_scripts="${clang_tidy_diff_scripts[@]}"
fi


clang_tidy_script=""
for script_option in ${search_scripts}
do
    echo $script_option
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

SRCDIR=$(bazel info workspace)
echo "Generating compilation database..."
pushd $SRCDIR

# This is a bit ugly, but limits the bazel build targets to only C++ code.
bazel_targets=$(bazel query 'kind("cc_(binary|test) rule",//... -//third_party/...)
               except attr("tags", "manual", //...)')

flags="--include_headers"
if [ "$build" = true ] ; then
  flags="$flags --run_bazel_build"
fi

# Bazel build need to be run to setup virtual includes, generating files which are consumed
# by clang-tidy.
"${SRCDIR}/scripts/gen_compilation_database.py" ${flags} ${bazel_targets}

# Actually invoke clang-tidy.
if [ "$diff_mode" = true ] ; then
    if [ -z "$diff_file" ] ; then
        git diff -U0 origin/main -- src | "${clang_tidy_script}" -p1 2>&1 | tee clang_tidy.log
    else
        cat ${diff_file} | "${clang_tidy_script}" -p1 2>&1 | tee clang_tidy.log
    fi
else
    "${clang_tidy_script}" -j $(nproc) src 2>&1 | tee clang_tidy.log
fi

popd
