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

# This script outputs a github actions matrix with each of the configs we want to build and test
# It takes the github event_name as its argument

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <event_name>"
  exit 1
fi
event_name="$1"
commit_message="$(git log "$(git merge-base origin/main HEAD)"..HEAD --format="%B")"

nightly_regression_test_iterations="${NIGHTLY_REGRESSION_TEST_ITERATIONS:-5}"

all_kernel_versions=(
  "4.14.254"
  "4.19.254"
  "5.10.173"
  "5.15.101"
  "5.4.235"
  "6.1.18"
)
default_kernel_versions=(
  "4.14.254"
  "6.1.18"
)
kernel_versions=( "${default_kernel_versions[@]}" )

check_tag() {
  tag="$1"
  echo "${commit_message}" | grep -P "${tag}([^\-a-zA-Z]|$)" > /dev/null
}

build_deps_flags=()
extra_bazel_args=()
# Build and test everything and BPF for main runs.
if [[ "${event_name}" == "push" ]]; then
  echo "Main run. Building all targets and bpf" >&2
  build_deps_flags+=("-a" "-b")
elif [[ "${event_name}" == "schedule" ]]; then
  echo "Nightly run. Building all targets and all kernel versions of bpf" >&2
  build_deps_flags+=("-a" "-b")
  extra_bazel_args+=("--runs_per_test=${nightly_regression_test_iterations}")
  kernel_versions=( "${all_kernel_versions[@]}" )
elif [[ "${event_name}" == "pull_request_target" ]] || [[ "${event_name}" == "pull_request" ]]; then
  # Ignore bazel dependency tracking and run all targets if #ci:ignore-deps is in the commit message.
  if check_tag '#ci:ignore-deps'; then
    echo "Found #ci:ignore-deps tag. Building all targets" >&2
    build_deps_flags+=("-a")
  fi
  # Build/Test bpf targets if #ci:bpf-build{-all-kernels} is in the commit message.
  if check_tag '#ci:bpf-build-all-kernels'; then
    echo "Found #ci:bpf-build-all-kernels tag. Building bpf targets on all kernel versions" >&2
    build_deps_flags+=("-b")
    kernel_versions=( "${all_kernel_versions[@]}" )
  elif check_tag '#ci:bpf-build'; then
    echo "Found #ci:bpf-build tag. Building bpf targets" >&2
    build_deps_flags+=("-b")
  fi
fi

# Create the target pattern files, passthrough flags to bazel_build_deps.
./ci/bazel_build_deps.sh "${build_deps_flags[@]}"
# TODO(james): generate the target pattern files directly from ci/github/matrix.yaml instead of using bazel_build_deps,
# so that we don't have to edit bazel_build_deps.sh for new configs.

remove_targetless_configs() {
  f="$1"
  # Remove any configs that don't have buildables.
  while read -r build_target_file;
  do
    if [ -s "${build_target_file}" ]; then
      continue
    fi
    yq -i e 'del(.configs[] | select(.buildables=="'"${build_target_file}"'"))' "${f}"
  done < <(yq e '.configs[].buildables' "${f}" | sort | uniq)

  # Remove the `tests` key from any configs where the test target file is empty.
  while read -r test_target_file;
  do
    if [ -s "${test_target_file}" ]; then
      continue
    fi
    yq -i e 'del(.configs[].tests | select(.=="'"${test_target_file}"'"))' "${f}"
  done < <(yq e '.configs[].tests' "${f}" | sort | uniq)
}

remove_disabled_configs() {
  yq -i e 'del(.configs[] | select(.disabled))' "$1"
  yq -i e 'del(.configs[].disabled)' "$1"
}

merge_yamls() {
  # shellcheck disable=SC2016
  yq eval-all '. as $item ireduce ({}; . *+ $item)' "$@"
}

extend_with_kernel_versions() {
  f="$1"
  kernel_matrix_configs="$(mktemp)"
  no_kernel="$(mktemp)"
  yq e 'del(.configs[] | select(.use_kernel_matrix != true))' "$f" > "${kernel_matrix_configs}"
  yq e 'del(.configs[] | select(.use_kernel_matrix))' "$f" > "${no_kernel}"

  version_files=()
  for version in "${kernel_versions[@]}"
  do
    version_configs="$(mktemp)"
    yq e '.configs[].args += " --//bazel/test_runners/qemu_with_kernel:kernel_version='"${version}"'"' "$kernel_matrix_configs" > "${version_configs}"
    yq -i e '.configs[].name += " ('"${version}"')"' "${version_configs}"
    version_files+=("${version_configs}")
  done

  merge_yamls "${version_files[@]}" "${no_kernel}" > "$f"
  yq -i e 'del(.configs[].use_kernel_matrix)' "$f"

  rm "${kernel_matrix_configs}"
  rm "${no_kernel}"
  for f in "${version_files[@]}"
  do
    rm "$f"
  done
}

add_extra_bazel_args() {
  f="$1"
  if [[ "${#extra_bazel_args[@]}" -gt 0 ]]; then
    yq -i e '.configs[].args += " '"${extra_bazel_args[*]}"'"' "$f"
  fi
}

process_matrix() {
  in="$1"
  out="$2"

  cp "${in}" "${out}"
  remove_disabled_configs "${out}"
  remove_targetless_configs  "${out}"
  add_extra_bazel_args "${out}"
  extend_with_kernel_versions "${out}"
}

matrix="$(mktemp)"
process_matrix "ci/github/matrix.yaml" "${matrix}"


if [[ "$(yq e '.configs|length' "${matrix}")" != 0 ]]; then
  yq e '.include = .configs | del(.configs)' -I=0 -o=json "${matrix}"
fi

rm "${matrix}"
