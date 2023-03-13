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

trap exit INT

PWD="$(pwd)"
pwd_resolved="$(readlink -f "${PWD}")"

# TODO(james): find a more reliable way to find execroot, for now we use pwd and look for an execroot directory.
# This approach is somewhat fragile, since if someone creates a directory called execroot, it will break.
execroot="${pwd_resolved%/${pwd_resolved##*/execroot/}}"
output_base="$(realpath "${execroot}/..")"

top_of_repo=""
for dir in "${output_base}"/execroot/px/*/; do
  if [ -L "${dir%/}" ]
  then
    resolved_dir=$(readlink -f "${dir%/}")
    top_of_repo="$(dirname "${resolved_dir}")"
    break
  fi
done

# Bazel sets up symlinks only on the files themselves, so find the real sysroot directory using files we know will exist.
sysroot_path="$(realpath "$(dirname "$(readlink -f "${PWD}/%sysroot%/usr/include/stdio.h")")/../..")"

chroot_dir="$TEST_TMPDIR/chroot";

chroot_script_internal_name="/scripts/inside_chroot.sh"
chroot_script="$(mktemp -p "${TEST_TMPDIR}")"
cat > "${chroot_script}" <<EOF
  cd ${PWD}
  $@
EOF

chroot_bash_args=()
if [ "${DEBUG_CHROOT}" = true ]; then
  # Instead of running the test, print the way to invoke the test.
  # If we run the test and the test segfaults the user will not get a shell in the chroot,
  # which defeats the purpose of DEBUG_CHROOT.
  sed -i '$ d' "${chroot_script}"
  {
    echo "echo '---------------------------------------'";
    echo "echo '- Command to run test: $*'";
    echo "echo '---------------------------------------'";
  } >> "${chroot_script}"
  chroot_bash_args+=("--init-file")
fi
chroot_bash_args+=("${chroot_script_internal_name}")

unshare_script="$(mktemp -p "${TEST_TMPDIR}")"
cat > "${unshare_script}" <<EOF
  bazel/test_runners/sysroot_chroot/setup_unshare.sh "${chroot_dir}" "${sysroot_path}" "${output_base}" "${top_of_repo}"

  # Copy the chroot script into the chroot directory.
  mkdir -p "${chroot_dir}/scripts"
  cp "${chroot_script}" "${chroot_dir}${chroot_script_internal_name}"

  # Change the shell to bash since we don't want to use the user's SHELL setting.
  export SHELL=/bin/bash
  # TODO(james): maybe don't rely on system chroot (although it should be present on most systems).
  /usr/sbin/chroot "${chroot_dir}" /bin/bash ${chroot_bash_args[@]}
EOF

unshare_bash_args=()
if [ "${DEBUG_UNSHARE}" = true ]; then
  unshare_bash_args+=("--init-file")
fi
unshare_bash_args+=("${unshare_script}")

echo "Running test in chroot with sysroot: ${sysroot_path}"

set +e
# -m creates a new mount namespace
# -r runs the script as a fake root
# -p creates a new PID namespace
# -f runs the script in a fork instead of directly through unshare (this seems to be necessary for the PID namespace to work properly)
# We currently don't create a network namespace for the unshare environment. Once we use podman, we should be able to use an isolated network namespace.
unshare -mrpf bash "${unshare_bash_args[@]}"
exit_code="$?"
set -e

if [[ "${exit_code}" != 0 ]] && [[ "${DEBUG_CHROOT}" != true ]] && [[ "${DEBUG_UNSHARE}" != true ]]; then
  echo "---------------------------------------------------------------"
  echo "  To drop into a shell in the chroot, run: "
  echo "    (pushd ${PWD}; \\"
  echo "    export TEST_TMPDIR=\`mktemp -d\`; \\"
  echo "    export TEST_SRCDIR=$TEST_SRCDIR; \\"
  echo "    DEBUG_CHROOT=true  bazel/test_runners/sysroot_chroot/test_runner.sh $*; \\"
  echo "    popd; rm -rf \"\$TEST_TMPDIR\"; unset TEST_TMPDIR; unset TEST_SRCDIR)"
  echo "---------------------------------------------------------------"
fi

rm "${unshare_script}"
rm "${chroot_script}"

exit "${exit_code}"
