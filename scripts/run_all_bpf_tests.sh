#! /bin/bash

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
echo "Running bazel query to find bpf tests..."
mapfile -t bpf_tests < <( bazel cquery --config=bpf --output=starlark 'kind(test, deps(//...)) except kind(sh_test, deps(//...))' 2>/dev/null )
mapfile -t bpf_sh_tests < <( bazel cquery --config=bpf --output=starlark 'kind(sh_test, deps(//...))' 2>/dev/null )

message() {
  echo "------------------------------------------------------------------------------------"
  echo "| $1"
  echo "------------------------------------------------------------------------------------"
}

passed_tests=""
success() {
  message "Passed $1"
  passed_tests="${passed_tests}$1"$'\n'
}
failed_tests=""
failure() {
  message "Failed $1"
  failed_tests="${failed_tests}$1"$'\n'
}

trap on_exit INT
on_exit() {
  sleep 1

  num_passed="$(echo -n "${passed_tests}" | grep -c '^')"
  num_failed="$(echo -n "${failed_tests}" | grep -c '^')"

  if [[ "${num_passed}" -gt 0 ]]; then
    message "Passed Tests"
    echo "${passed_tests}"
  fi
  if [[ "${num_failed}" -gt 0 ]]; then
    message "Failed Tests"
    echo "${failed_tests}"
  fi

  message "Passed: ${num_passed}"$'\t'"Failed: ${num_failed}"
  if [[ "${num_failed}" -eq 0 ]] && [[ "${num_passed}" == "${#bpf_tests[@]}" ]]; then
    exit 0
  else
    exit 1
  fi
}

for target in "${bpf_tests[@]}";
do
  message "Running ${target}";
  if ./scripts/sudo_bazel_run.sh "${target}"; then
    success "${target}"
  else
    failure "${target}"
  fi
done

for target in "${bpf_sh_tests[@]}";
do
  message "Running ${target}";
  bazel build --remote_download_outputs=all "${target}";
  target_executable=$(bazel cquery "${target}" --output starlark --starlark:expr "target.files.to_list()[0].path" 2>/dev/null)

  env_args=()
  if [[ -f "${target_executable}.runfiles/MANIFEST" ]]; then
    env_args+=("RUNFILES_MANIFEST_FILE=${target_executable}.runfiles/MANIFEST")
  elif [[ -f "${target_executable}.runfiles_manifest" ]]; then
    env_args+=("RUNFILES_MANIFEST_FILE=${target_executable}.runfiles_manifest")
  fi

  if [[ -d "${target_executable}.runfiles/" ]]; then
    env_args+=("RUNFILES_DIR=${target_executable}.runfiles/")
  fi

  if "${env_args[@]}" "$target_executable"; then
    success "${target}"
  else
    failure "${target}"
  fi
done

on_exit
