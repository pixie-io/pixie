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

DISK_SIZE="4096M"
FS_TYPE="ext4"
OUTPUT_DISK=""
SYSROOT=""
EXTRAS=""

usage() {
    echo "Usage: $0 -o <output_disk> -s <sysroot tar.gz> -b <busybox> -e <src>:<dest>,<src2>:<dest2> -k <kernel_package>"
    echo "       <output_disk>           The generated ext2fs file system image as a qcow2 file"
    echo "       <sysroot.tar.gz>        The input sysroot to use for the disk"
    echo "       <additional_files>      Additional files that need to be written to the image"
    echo "       <kernel package>        The tar.gz package of kernel and header files."
    exit 1
}

parse_args() {
  local OPTIND
  while getopts "o:e:s:b:k:h" opt; do
    case ${opt} in
      o)
	OUTPUT_DISK=$OPTARG
        ;;
      e)
	EXTRAS=$OPTARG
        ;;
      s)
        SYSROOT=$OPTARG
        ;;
      b)
	BUSYBOX=$OPTARG
	;;
      k)
	KERNEL_PACKAGE=$OPTARG
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
  shift $((OPTIND - 1))
}

parse_args "$@"

if ! command -v mke2fs &> /dev/null
then
  echo "mke2fs is a required command. You might be able to 'apt-get install libext2fs2'."
  exit
fi

if ! command -v qemu-img &> /dev/null
then
  echo "qemu-img is a required command. You might be able to 'apt-get install qemu-utils'."
  exit
fi

if [[ -z "${OUTPUT_DISK}" ]]; then
    echo "-o : output disk is a required argument"
    usage
fi

if [[ -z "${SYSROOT}" ]]; then
    echo "-s : sysroot is a required argument"
    usage
fi


if [[ -z "${BUSYBOX}" || ! -f "${BUSYBOX}" ]]; then
    echo "-b : busybox is a required argument and needs to point to an executable."
    usage
fi

if [[ -z "${KERNEL_PACKAGE}" || ! -f "${KERNEL_PACKAGE}" ]]; then
    echo "-k : kernel package is a required argument and needs to point to tar.gz file."
    usage
fi

SYSROOT="$(realpath "${SYSROOT}")"
OUTPUT_DISK="$(realpath "${OUTPUT_DISK}")"
BUSYBOX="$(realpath "${BUSYBOX}")"
KERNEL_PACKAGE="$(realpath "${KERNEL_PACKAGE}")"

# Need to remove disk to prevent mke2fs from becoming interactive.
if [[ -f "${OUTPUT_DISK}" ]]; then
    echo "Removing stale output disk: ${OUTPUT_DISK}."
    rm -f "${OUTPUT_DISK}"
fi

echo "Config:"
echo "  sysroot: ${SYSROOT}"
echo "  extras:  ${EXTRAS}"
echo "  output: ${OUTPUT_DISK}"

# Extract the sysroot tar.gz to a tmp folder and copy
# in the extra files.
build_dir=$(mktemp -d)
sysroot_build_dir="${build_dir}/sysroot"
mkdir -p "${sysroot_build_dir}"

tar -C "${sysroot_build_dir}" -xf "${SYSROOT}"

# Extract the kernel modules.
tar -C "${sysroot_build_dir}" -xf "${KERNEL_PACKAGE}" \
  --strip-components=2 pkg/root

# Copy over required extra files.
IFS=',' read -ra extra_files <<< "${EXTRAS}"
for f in "${extra_files[@]}"; do
    IFS=':' read -r from to <<< "${f}"
    cp "${from}" "${sysroot_build_dir}/${to}"
done


cp "${BUSYBOX}" "${sysroot_build_dir}/bin/busybox"
chmod +x "${sysroot_build_dir}/bin/busybox"

tmpdisk_image="$(mktemp  --suffix .img)"

# We need the files to be owned by root so we unshare
# and then chown the sysroot files before building the FS.
unshare -r bash <<EOF

chroot "${sysroot_build_dir}" /bin/busybox --install -s /bin

chown -R 0:0 "${sysroot_build_dir}"

# Actually create the file system.
mke2fs \
  -q \
  -L '' \
  -O ^64bit \
  -d "${sysroot_build_dir}" \
  -m 5 \
  -r 1 \
  -t "${FS_TYPE}" \
  -E root_owner=0:0 \
  "${tmpdisk_image}" \
  "${DISK_SIZE}"

EOF

qemu-img convert \
  -f raw -O qcow2 \
  "${tmpdisk_image}" "${OUTPUT_DISK}"

rm -f "${tmpdisk_image}"
