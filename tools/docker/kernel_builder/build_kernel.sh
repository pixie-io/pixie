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

usage() {
    echo "Usage: build_kernel.sh -v <kernel_version> -c <config_file> -w <work_directory>"
    echo "  The work directory will have the following structure: "
    echo "       linux-<kernel_version>/ The downloaded kernel source"
    echo "       build/                  The directory where the kernel is built"
    echo "       pkg/                    The package directory of kernel, modules and"
    echo "                                   required source artifacts."
    exit 1
}

parse_args() {
  local OPTIND
  while getopts "v:c:w:h" opt; do
    case ${opt} in
      v)
        KERNEL_VERSION=$OPTARG
        ;;
      c)
        CONFIG_FILE=$OPTARG
        ;;
      w)
        WORK_DIR=$OPTARG
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

create_unlinked_dir() {
    d="$1"
    u="${d}.u"

    mkdir -p "${u}"
    rsync -rL "${d}"/* "${u}/"
    rm -rf "${d}"
    mv "${u}" "${d}"
}

parse_args "$@"

if [[ -z "${KERNEL_VERSION}" ]]; then
    echo "kernel version -k is required"
    usage
fi

if [[ -z "${CONFIG_FILE}" ]]; then
    echo "kernel config -c is required"
    usage
fi

if [[ -z "${WORK_DIR}" ]]; then
    echo "work directory -w is required"
    usage
fi

echo "Building in ${WORK_DIR}"

major_version=$(echo "${KERNEL_VERSION}" | cut -d . -f 1)
kernel_tar_xz_name="linux-${KERNEL_VERSION}.tar.xz"
kernel_source_dir="${WORK_DIR}/linux-${KERNEL_VERSION}"
build_dir="${WORK_DIR}/build"
pkg_dir="${WORK_DIR}/pkg"

mkdir -p "${build_dir}"
mkdir -p "${pkg_dir}/root"

wget "https://cdn.kernel.org/pub/linux/kernel/v${major_version}.x/${kernel_tar_xz_name}"
tar xf "${kernel_tar_xz_name}"

pushd "${kernel_source_dir}"
make O="${build_dir}" allnoconfig
popd # kernel_source_dir

cp "${CONFIG_FILE}" "${build_dir}"/.config

pushd "${build_dir}"
make olddefconfig
make -j "$(nproc)"
cp arch/x86/boot/bzImage "${pkg_dir}"

# This will create <pkg_dir>/root/lib...
make INSTALL_MOD_PATH="${pkg_dir}/root" modules_install
popd # build_dir

# Now we need to do some surgery to the installed headers.
#    1. Make build/source not a symlink.
#    2. Purge files that we don't need from build/source.
module_path="${pkg_dir}/root/lib/modules/${KERNEL_VERSION}"
module_build_path="${module_path}/build"
module_source_path="${module_path}/source"

create_unlinked_dir "${module_build_path}"
create_unlinked_dir "${module_source_path}"

# Now remove the files that we don't need.
find "${module_build_path}" -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d  -exec rm -rf {} +
find "${module_build_path}" ! -name '*.h' -type f -exec rm -f {} +

find "${module_source_path}" -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d  -exec rm -rf {} +
find "${module_source_path}"/arch -maxdepth 1 -mindepth 1 ! -name x86 -type d -exec rm -rf {} +
find "${module_source_path}"/arch ! -name '*.h' -type f -exec rm -f {} +
find "${module_source_path}"/include ! -name '*.h' -type f -exec rm -f {} +

tar -czf "${WORK_DIR}/pkg.tar.gz" -C "${WORK_DIR}" pkg
