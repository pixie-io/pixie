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

set -e

KERNEL_IMAGE=%kernelimage%
DISK_IMAGE=%diskimage%
BAZEL_QEMU_TEST_RUNNER_PATH=%testrunnerinsideqemu%
RUN_QEMU_SCRIPT=%runqemuscript%

# Create a tmp directory that serves as the /test_fs sanbox dir inside qemu.
tmpdir_for_sandbox=$(mktemp -d)

function cleanup {
  retval=$?
  rm -rf "${tmpdir_for_sandbox:?}" || true
  exit "${retval}"
}

# We set a trap to make sure the tmp directories are cleaned up.
trap cleanup EXIT
trap cleanup ERR

function strip_pwd_from_path() {
  v=$1
  v=${v//"${OLDPWD}"/""}
  echo "$v"
}

# Replaces the PWD inside qemu with /test_fs.
function path_inside_qemu() {
  v=$1
  test_base="/test_fs"
  v=${v//"${OLDPWD}"/"${test_base}"}
  echo "$v"
}

# Replaces the PWD outside qemu with a path to the sandbox directory.
function path_qemu_sandbox() {
  v=$1
  v=${v//"${OLDPWD}"/"${tmpdir_for_sandbox}"}
  echo "$v"
}


# We need to write and transform the environment variables so that they have the "correct"
# paths when run inside of qemu.
test_env_file="${tmpdir_for_sandbox}/test_env.sh"
echo "#!/bin/bash" > "${test_env_file}"

# Grab the environment variables and rewrite them for the qemu sandbox.
env -0 | while IFS='=' read -r -d '' n v; do
  # Writing bash funcs as env variables doesn't work. Luckily we don't actually need them.
  if [[ "${n}" == BASH_FUNC* ]]; then
    continue
  fi

  # We don't want to write out the PWD since it will be wrong inside of qemu.
  if [[ "${n}" == PWD ]]; then
    continue
  fi

  # All paths will basically start with /test_fs.
  v=$(path_inside_qemu "$v")

  printf "export %s=%s\n" "$n" "$v" >> "${test_env_file}"
done


# Copy the runfiles into the sandbox directory. 9p fs does not like symlinks
# so we resolve them into the copy.
runfiles_path="$(path_qemu_sandbox "${RUNFILES_DIR}")"
mkdir -p "${runfiles_path}"
cp -afL "${RUNFILES_DIR}"/* "${runfiles_path}"

function rewrite_manifest() {
  awk '{print $1" '"$(path_inside_qemu "${RUNFILES_DIR}")/"'"$1}' "$1"
}

if [ -f "${RUNFILES_DIR}/MANIFEST" ]; then
  rewrite_manifest "${RUNFILES_DIR}/MANIFEST" > "${runfiles_path}/MANIFEST"
fi
if [ -f "${RUNFILES_DIR}_manifest" ]; then
  rewrite_manifest "${RUNFILES_DIR}_manifest" > "${runfiles_path}_manifest"
fi

# Copy over testlog file.
testlogs_dir="${TEST_WARNINGS_OUTPUT_FILE%%/testlogs/*}/testlogs/"
qemu_warnings_file=$(strip_pwd_from_path "${TEST_WARNINGS_OUTPUT_FILE}")
qemu_testlogs_dir="${qemu_warnings_file/${qemu_warnings_file##*/testlogs/}}"
cp -afL "${testlogs_dir}/" "${tmpdir_for_sandbox}/${qemu_testlogs_dir}/"

# Copy the test runner and test cmd into the sandbox.
test_runner_file="${tmpdir_for_sandbox}/test_runner_inside_qemu.sh"
test_base=${PWD//"${OLDPWD}"/\/test_fs}
test_cmd_path="${tmpdir_for_sandbox}/test_cmd.sh"
test_cmd_path_in_qemu="/test_fs/test_cmd.sh"

echo "#!/bin/bash -e" > "${test_cmd_path}"
echo "${@:1}" >> "${test_cmd_path}"
chmod +x "${test_cmd_path}"

printf "export test_base=%s\n" "${test_base}" >> "${test_env_file}"
printf "export test_exec_path=%s\n" "${test_cmd_path_in_qemu}" >> "${test_env_file}"

cp -afL "${BAZEL_QEMU_TEST_RUNNER_PATH}" "${test_runner_file}"

# Launch the qemu test.
retval=0
(exec env - \
  QEMU_TEST_FS_PATH="${tmpdir_for_sandbox}" \
  QEMU_KERNEL_IMAGE="${KERNEL_IMAGE}" \
  QEMU_DISK_BASE_RO="${PWD}/${DISK_IMAGE}" \
  "${RUN_QEMU_SCRIPT}" ) || retval=$?

# We use a known path to find the testlogs directory so that we can copy the results back from qemu.
testlogs_dir="${TEST_WARNINGS_OUTPUT_FILE%%/testlogs/*}/testlogs/"
qemu_warnings_file=$(strip_pwd_from_path "${TEST_WARNINGS_OUTPUT_FILE}")
qemu_testlogs_dir="${qemu_warnings_file/${qemu_warnings_file##*/testlogs/}}"

cp -afL "${tmpdir_for_sandbox}/${qemu_testlogs_dir}/" "${testlogs_dir}/"

exit $retval
