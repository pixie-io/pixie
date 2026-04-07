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
wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v"${KERN_MAJ}".x/linux-"${KERN_VERSION}".tar.gz

tar zxf linux-"${KERN_VERSION}".tar.gz

pushd linux-"${KERN_VERSION}" || exit

LOCALVERSION="-pl"

cp /configs/"${ARCH}" .config
make ARCH="${ARCH}" olddefconfig

# Only generate headers — no kernel or module compilation needed.
# 'make prepare' generates include/generated/ and arch/*/include/generated/
# which are the only outputs we package.
echo "Generating headers for ${KERN_VERSION}${LOCALVERSION} (${ARCH})"
make ARCH="${ARCH}" prepare LOCALVERSION="${LOCALVERSION}"

popd || exit
popd || exit

# Package headers into the same directory structure the old deb-pkg approach produced
# (usr/src/linux-headers-<version><localversion>/{include,arch}).
KERNEL_ARCH="${ARCH//x86_64/x86}"
HEADERS_DIR="usr/src/linux-headers-${KERN_VERSION}${LOCALVERSION}"

mkdir -p "${HEADERS_DIR}/arch"
cp -a "src/linux-${KERN_VERSION}/include" "${HEADERS_DIR}/"
cp -a "src/linux-${KERN_VERSION}/arch/${KERNEL_ARCH}" "${HEADERS_DIR}/arch/"

# Remove broken symlinks
find "${HEADERS_DIR}" -xtype l -exec rm {} +

tar zcf linux-headers-"${ARCH}"-"${KERN_VERSION}".tar.gz usr

cp linux-headers-*.tar.gz "${OUTPUT_DIR}"/
