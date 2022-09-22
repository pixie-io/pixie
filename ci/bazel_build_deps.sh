#!/bin/bash -ex

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

# This script finds all the Bazel targets based on which files are changed and flags passed in.

# Go to the root of the repo
cd "$(git rev-parse --show-toplevel)" || exit

bazel_query="bazel query --keep_going --noshow_progress"

# A list of patterns that will trigger a full build.
poison_patterns=('^Jenkinsfile' '^bazel\/' '^ci\/' '^docker\.properties' '^\.bazelrc' '^BUILD.bazel')

# A list of patterns that will guard BPF targets.
# We won't run BPF targets unless there are changes to these patterns.
bpf_patterns=('^src\/stirling\/')

# Set the default values for the flags.
all_targets=false
# Enable running of BPF targets.
run_bpf_targets=false

commit_range=$(git merge-base origin/main HEAD)

experimental_exclude=""
if [[ -d experimental ]]; then
    experimental_exclude="except //experimental/..."
fi

ui_excludes="except //src/ui/..."
bpf_excludes="except attr('tags', 'requires_bpf', //...)"
go_xcompile_excludes="except //src/pixie_cli:px_darwin_amd64 except //src/pixie_cli:px_darwin_arm64"
default_excludes="except attr('tags', 'manual', //...) \
  except //third_party/... ${experimental_exclude}"

sanitizer_only="except attr('tags', 'no_asan', //...) \
  except attr('tags', 'no_msan', //...) \
  except attr('tags', 'no_tsan', //...)"

function usage() {
  echo "Usage: $0 [ -a ]" >&2
    echo " -a all_targets=${all_targets}" >&2
}

while getopts "abh" option; do
  case "${option}" in
    a )
       all_targets=true
       ;;
    b )
       run_bpf_targets=true
       ;;
    h ) # "help"
       usage
       exit 0
       ;;
    \?)
       echo "Option '-$OPTARG' is not a valid option." >&2
       usage
       exit 1
       ;;
  esac
done

# There are several types of builds and they each need a list of deps to build and
# test to run:
#     Clang:opt + UI
#     Clang:dbg
#     GCC:opt
#     ASAN/TSAN
#     BPF
#     BPF ASAN/TSAN
#
# This list needs to be kept in sync between the Jenkinsfile, .bazelrc and this file.
# Query for the associated buildables & tests and write them to a file:
#     bazel_{buildables, tests}_clang_opt
#     bazel_{buildables, tests}_clang_dbg
#     bazel_{buildables, tests}_gcc_opt
#     bazel_{buildables, tests}_sanitizer
#     bazel_{buildables, tests}_bpf
#     bazel_{buildables, tests}_bpf_sanitizer

targets=()
function compute_targets() {
  if [ "${all_targets}" = "true" ]; then
    targets=("//...")
    return 0
  fi

  changed_files=()
  for file in $(git diff --name-only "${commit_range}" ); do
    for pat in "${poison_patterns[@]}"; do
      if [[ "$file" =~ ${pat} ]]; then
        echo "File ${file} with ${pat} modified. Triggering full build"
        targets=("//...")
        return 0
      fi
    done

    if [[ "$file" =~ (.*)/BUILD.bazel && -f "$file" ]]; then
      changed_files+=("${BASH_REMATCH[1]}/...")
    else
      # The following lines check to see if this file is used by
      # any bazel targets and skip it otherwise.
      # This filtering ensures that rdeps doesn't fail.
      ret=0
      bazel query --noshow_progress "$file" 1>/dev/null 2>/dev/null || ret=$?
      if [[ ret -eq 0 ]]; then
        changed_files+=("$file")
      fi
    fi
  done

  targets+=("rdeps(//..., set(${changed_files[*]}))")
}

function check_bpf_trigger() {
  for file in $(git diff --name-only "${commit_range}" ); do
    for pat in "${bpf_patterns[@]}"; do
      if [[ "$file" =~ ${pat} ]]; then
        echo "File ${file} with ${pat} modified. Triggering bpf targets"
        run_bpf_targets=true
        return 0
      fi
    done
  done
}

compute_targets
check_bpf_trigger

buildables="kind(.*_binary, ${targets}) union kind(.*_library, ${targets}) ${default_excludes}"
tests="kind(test, ${targets}) ${default_excludes}"

cc_buildables="kind(cc_.*, ${buildables})"
cc_tests="kind(cc_.*, ${tests})"

go_buildables="kind(go_.*, ${buildables})"
go_tests="kind(go_.*, ${tests})"

bpf_buildables="attr('tags', 'requires_bpf', ${buildables})"
bpf_tests="attr('tags', 'requires_bpf', ${tests})"

cc_bpf_buildables="kind(cc_.*, ${bpf_buildables})"
cc_bpf_tests="kind(cc_.*, ${bpf_tests})"


# Clang:opt (includes non-cc targets: go targets, //src/ui/..., etc.)
${bazel_query} "${buildables} ${bpf_excludes}" > bazel_buildables_clang_opt 2>/dev/null
${bazel_query} "${tests} ${bpf_excludes}" > bazel_tests_clang_opt 2>/dev/null

# Clang:dbg
${bazel_query} "${cc_buildables} ${bpf_excludes}" > bazel_buildables_clang_dbg 2>/dev/null
${bazel_query} "${cc_tests} ${bpf_excludes}" > bazel_tests_clang_dbg 2>/dev/null

# GCC:opt
${bazel_query} "${cc_buildables} ${bpf_excludes}" > bazel_buildables_gcc_opt 2>/dev/null
${bazel_query} "${cc_tests} ${bpf_excludes}" > bazel_tests_gcc_opt 2>/dev/null

# Sanitizer (Limit to C++ only).
${bazel_query} "${cc_buildables} ${bpf_excludes} ${sanitizer_only}" > bazel_buildables_sanitizer 2>/dev/null
${bazel_query} "${cc_tests} ${bpf_excludes} ${sanitizer_only}" > bazel_tests_sanitizer 2>/dev/null

if [ "${run_bpf_targets}" = "true" ]; then
  # BPF.
  ${bazel_query} "${bpf_buildables}" > bazel_buildables_bpf 2>/dev/null
  ${bazel_query} "${bpf_tests}" > bazel_tests_bpf 2>/dev/null

  # BPF Sanitizer (C/C++ Only, excludes shell tests).
  ${bazel_query} "${cc_bpf_buildables} ${sanitizer_only}" > bazel_buildables_bpf_sanitizer 2>/dev/null
  ${bazel_query} "${cc_bpf_tests} ${sanitizer_only}" > bazel_tests_bpf_sanitizer 2>/dev/null
else
  # BPF.
  cat /dev/null > bazel_buildables_bpf
  cat /dev/null > bazel_tests_bpf

  # BPF Sanitizer (C/C++ Only, excludes shell tests).
  cat /dev/null > bazel_buildables_bpf_sanitizer
  cat /dev/null > bazel_tests_bpf_sanitizer
fi

# Should we run clang-tidy?
${bazel_query} "${cc_buildables}" > bazel_buildables_clang_tidy 2>/dev/null
${bazel_query} "${cc_tests}" > bazel_tests_clang_tidy 2>/dev/null

# Should we run golang race detection?
${bazel_query} "${go_buildables} ${go_xcompile_excludes}" > bazel_buildables_go_race 2>/dev/null
${bazel_query} "${go_tests} ${go_xcompile_excludes}" > bazel_tests_go_race 2>/dev/null

# Should we run doxygen?
bazel_cc_touched=$(${bazel_query} "${cc_buildables} union ${cc_tests}" 2>/dev/null)
if [ "${all_targets}" = "true" ] || [[ -z $bazel_cc_touched ]]; then
  touch run_doxygen
fi
