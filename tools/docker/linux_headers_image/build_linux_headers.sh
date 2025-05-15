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

usage() {
    echo "Usage: $0 [workspace] [kernel_version] [arch] [output_dir]"
    echo "Example: $0 /px 5.4.0-42-generic x86_64 /output"
    echo "If cross-compiling, set the CROSS_COMPILE environment variable to the cross-compiler prefix."
}

if [ "$#" -ne 4 ]; then
    echo "Invalid number of arguments. Expected 4, but received $# ($*)"
    usage
    exit 1
fi

WORKSPACE=$1
KERN_VERSION=$2
ARCH=$3
OUTPUT_DIR=$4

if [ -z "${WORKSPACE}" ] || [ -z "${KERN_VERSION}" ] || [ -z "${ARCH}" ] || [ -z "${OUTPUT_DIR}" ]; then
    usage
    exit 1
fi

if [ "${ARCH}" != "$(uname -m)" ]; then
    if [ -z "${CROSS_COMPILE}" ]; then
        echo "CROSS_COMPILE is not set. Please set it to the cross-compiler prefix."
        exit 1
    fi
fi

mkdir -p "${WORKSPACE}"/src
pushd "${WORKSPACE}"/src || exit

KERN_MAJ=$(echo "${KERN_VERSION}" | cut -d'.' -f1);
KERN_MIN=$(echo "${KERN_VERSION}" | cut -d'.' -f2);
wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v"${KERN_MAJ}".x/linux-"${KERN_VERSION}".tar.gz

tar zxf linux-"${KERN_VERSION}".tar.gz

pushd linux-"${KERN_VERSION}" || exit

cp /configs/"${ARCH}" .config
make ARCH="${ARCH}" olddefconfig
make ARCH="${ARCH}" clean

LOCALVERSION="-pl"

DEB_ARCH="${ARCH//x86_64/amd64}"
# binary builds are required for non git trees after linux v6.3 (inclusive).
# The .deb file suffix is also different.
TARGET='bindeb-pkg'
DEB_SUFFIX="-1_${DEB_ARCH}.deb"
if [ "${KERN_MAJ}" -lt 6 ] || { [ "${KERN_MAJ}" -le 6 ] && [ "${KERN_MIN}" -lt 3 ]; }; then
    TARGET='deb-pkg'
    DEB_SUFFIX="${LOCALVERSION}-1_${DEB_ARCH}.deb"
fi
echo "Building ${TARGET} for ${KERN_VERSION}${LOCALVERSION} (${ARCH})"

make ARCH="${ARCH}" -j "$(nproc)" "${TARGET}" LOCALVERSION="${LOCALVERSION}"

popd || exit
popd || exit

# Extract headers into a tarball
dpkg -x src/linux-headers-"${KERN_VERSION}${LOCALVERSION}_${KERN_VERSION}${DEB_SUFFIX}" .

# Remove broken symlinks
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}" -xtype l -exec rm {} +

# Remove uneeded files to reduce size
# Keep only:
# - usr/src/linux-headers-x.x.x-pl/include
# - usr/src/linux-headers-x.x.x-pl/arch/${ARCH}
# This reduces the size by a little over 2x.
rm -rf usr/share
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}" -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d \
    -exec rm -rf {} +
find usr/src/linux-headers-"${KERN_VERSION}${LOCALVERSION}"/arch -maxdepth 1 -mindepth 1 ! -name "${ARCH//x86_64/x86}" -type d -exec rm -rf {} +

tar zcf linux-headers-"${ARCH}"-"${KERN_VERSION}".tar.gz usr

cp linux-headers-*.tar.gz "${OUTPUT_DIR}"/
