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

chroot_dir="$1"
sysroot_path="$2"
output_base="$3"
top_of_repo="$4"

bind_mount() {
  src="$1"
  dst="$2"
  if [ -d "${src}" ]; then
    mkdir -p "${dst}"
  else
    mkdir -p "$(dirname "${dst}")"
    touch "${dst}"
  fi
  mount --bind "${src}" "${dst}"
}

overlay_mount() {
  src="$1"
  dst="$2"
  overlay_dir="$3"
  upper="${overlay_dir}/upper"
  workdir="${overlay_dir}/workdir"

  mkdir -p "${overlay_dir}"
  mount -t tmpfs tmpfs "${overlay_dir}"
  mkdir -p "${upper}"
  mkdir -p "${workdir}"
  mkdir -p "${dst}"
  mount -t overlay overlay -o "xino=off,lowerdir=${src},upperdir=${upper},workdir=${workdir}" "${dst}"
}


sysroot_dirs=("lib" "usr" "bin" "etc" "var" "sbin" "run")
if [ -d "${sysroot_path}/lib64" ]; then
  sysroot_dirs+=("lib64")
fi

# Mount all the sysroot dirs.
for dir in "${sysroot_dirs[@]}"
do
  bind_mount "${sysroot_path}/${dir}" "${chroot_dir}/${dir}"
done

# Mount docker socket.
bind_mount /var/run/docker.sock "${chroot_dir}/var/run/docker.sock"

# Mount repo, bazel output base as overlays, so that the actual repo and output_base are not modified.
output_base_overlay="$TEST_TMPDIR/output_base_overlay";
overlay_mount "${output_base}" "${chroot_dir}/${output_base}" "${output_base_overlay}"

if [ -n "${top_of_repo}" ]
then
  repo_overlay="$TEST_TMPDIR/repo_overlay";
  overlay_mount "${top_of_repo}" "${chroot_dir}/${top_of_repo}" "${repo_overlay}"
fi

# Mount a new tmpfs for the test tmpdir.
mkdir -p "${chroot_dir}/tmp"
mount -t tmpfs tmpfs "${chroot_dir}/tmp"
export TEST_TMPDIR=/tmp

# Mount system dirs
mkdir -p "${chroot_dir}/dev"
mount --rbind /dev "${chroot_dir}/dev"
mkdir -p "${chroot_dir}/sys"
mount --rbind /sys "${chroot_dir}/sys"
mkdir -p "${chroot_dir}/proc"
mount -t proc /proc "${chroot_dir}/proc"


# Miscellaneous setup tasks
echo "127.0.0.1 localhost" > "${chroot_dir}/etc/hosts"
echo "root:x:0:0:root:/root:/bin/bash" > "${chroot_dir}/etc/passwd"
export SHELL=/bin/bash
ln -snf /usr/bin/gawk "${chroot_dir}/usr/bin/awk"
