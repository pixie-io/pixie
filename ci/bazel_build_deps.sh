#!/bin/bash -e
# This script finds all the Bazel targets based on which files are changed and flags passed in.

# Go to the root of the repo
cd "$(git rev-parse --show-toplevel)" || exit

bazel_query="bazel query --keep_going --noshow_progress"

# A list of patterns that will trigger a full build.
poison_patterns=('^Jenkinsfile' '^ci\/' '^docker\.properties' '^.bazelrc')

# Set the default values for the flags.
all_targets=false
target_pattern="//..."
commit_range=${commit_range:-$(git merge-base origin/main HEAD)".."}

ui_excludes="except //src/ui/..."
bpf_excludes="except attr('tags', 'requires_bpf', //...)"
default_excludes="except attr('tags', 'manual', //...) \
  except //third_party/... \
  except //experimental/... \
  except //demos/..."
sanitizer_only="except attr('tags', 'no_asan', //...) \
  except attr('tags', 'no_msan', //...) \
  except attr('tags', 'no_tsan', //...)"

usage() {
  echo "Usage: $0 [ -a -t <target_pattern> -c <commit_range> ]" >&2
    echo " -a all_targets=${all_targets}" >&2
    echo " -t target_pattern=${target_pattern}" >&2
    echo " -c commit_range=${commit_range}" >&2

}

while getopts "aht:c:" option; do
  case "${option}" in
    a )
       all_targets=true
       ;;
    c )
       commit_range="$OPTARG.."
       ;;
    t )
       target_pattern=$OPTARG
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
    : )
       echo "Option '-$OPTARG' needs an argument." >&2
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

buildables_kind="kind(.*_binary, //...) ${default_excludes}"
tests_kind="kind(test, //...) ${default_excludes}"

if [ "${all_targets}" = "false" ]; then
  for file in $(git diff --name-only "${commit_range}" ); do
    for pat in "${poison_patterns[@]}"; do
      if [[ "$file" =~ ${pat} ]]; then
        echo "File ${file} with ${pat} modified. Triggering full build"
        all_targets=true
        break 2
      fi
    done
  done
fi

if [ "${all_targets}" = "false" ]; then
  # Get a list of the current files in package form by querying Bazel.
  # This step is safe to run without looking at specific configs
  # because it just translates file names to bazel packages.
  files=()
  for file in $(git diff --name-only "${commit_range}" ); do
    # shellcheck disable=SC2207
    files+=($(bazel query --noshow_progress "$file" || true))
  done

  buildables_kind="kind(.*_binary, rdeps(${target_pattern}, set(${files[*]}))) ${default_excludes}"
  tests_kind="kind(test, rdeps(${target_pattern}, set(${files[*]}))) ${default_excludes}"
fi


# Clang:opt
${bazel_query} "${buildables_kind} ${bpf_excludes}" > bazel_buildables_clang_opt
${bazel_query} "${tests_kind} ${bpf_excludes}" > bazel_tests_clang_opt

# Clang:dbg
${bazel_query} "kind(cc_binary, ${buildables_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_buildables_clang_dbg
${bazel_query} "kind(cc_test, ${tests_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_tests_clang_dbg

# GCC:opt
${bazel_query} "kind(cc_binary, ${buildables_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_buildables_gcc_opt
${bazel_query} "kind(cc_test, ${tests_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_tests_gcc_opt

# Sanitizer (Limit to C++ only).
${bazel_query} "kind(cc_binary, ${buildables_kind}) ${ui_excludes} ${bpf_excludes} \
  ${sanitizer_only}" > bazel_buildables_sanitizer
${bazel_query} "kind(cc_test, ${tests_kind}) ${ui_excludes} ${bpf_excludes} \
  ${sanitizer_only}" > bazel_tests_sanitizer

# BPF.
${bazel_query} "attr('tags', 'requires_bpf', ${buildables_kind})" > bazel_buildables_bpf
${bazel_query} "attr('tags', 'requires_bpf', ${tests_kind})" > bazel_tests_bpf

# BPF Sanitizer (C/C++ Only).
${bazel_query} "kind(cc_binary, attr('tags', 'requires_bpf', ${buildables_kind})) \
  ${sanitizer_only}" > bazel_buildables_bpf_sanitizer
${bazel_query} "kind(cc_test, attr('tags', 'requires_bpf', ${tests_kind})) \
  ${sanitizer_only}" > bazel_tests_bpf_sanitizer

# Should we run clang-tidy?
${bazel_query} "kind(cc_binary, ${buildables_kind})" > bazel_buildables_clang_tidy
${bazel_query} "kind(cc_test, ${tests_kind})" > bazel_tests_clang_tidy

# Should we run golang race detection?
${bazel_query} "kind(go_binary, ${buildables_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_buildables_go_race
${bazel_query} "kind(go_test, ${tests_kind}) ${ui_excludes} ${bpf_excludes}" > bazel_tests_go_race

# Should we run doxygen?
bazel_buildables_cc=$(${bazel_query} "kind(cc_binary, ${buildables_kind})")
bazel_tests_cc=$(${bazel_query} "kind(cc_binary, ${buildables_kind})")
if [ "${all_targets}" = "true" ] || [[ -z $bazel_buildables_cc ]] || [[ -z $bazel_tests_cc ]]; then
  touch run_doxygen
fi
