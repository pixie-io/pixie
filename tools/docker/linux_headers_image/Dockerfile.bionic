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

FROM ubuntu:18.04@sha256:8aa9c2798215f99544d1ce7439ea9c3a6dfd82de607da1cec3a8a2fae005931b

# Install required packages
RUN apt-get update
RUN apt-get upgrade -y -q
RUN apt-get install -y -q build-essential \
  bc \
  libelf-dev \
  libssl-dev \
  flex \
  bison \
  kmod \
  cpio \
  rsync \
  wget \
  binutils-aarch64-linux-gnu \
  gcc-aarch64-linux-gnu \
  dwarves \
  python3

ARG KERN_VERSION

# Download Linux sources
WORKDIR /px/src
RUN KERN_MAJ="$(echo "${KERN_VERSION}" | cut -d'.' -f1)"; \
    wget -nv http://mirrors.edge.kernel.org/pub/linux/kernel/v${KERN_MAJ}.x/linux-${KERN_VERSION}.tar.gz
RUN tar zxf linux-${KERN_VERSION}.tar.gz

WORKDIR /configs
ADD x86_64_config /configs/x86_64
ADD arm64_config /configs/arm64

ARG ARCH
ARG CROSS_COMPILE

# Build Linux kernel
WORKDIR /px/src/linux-${KERN_VERSION}
RUN cp /configs/${ARCH} .config
RUN make ARCH=${ARCH} olddefconfig
RUN make ARCH=${ARCH} clean
RUN make ARCH=${ARCH} -j $(nproc) deb-pkg LOCALVERSION=-pl

# Extract headers into a tarball
WORKDIR /px
RUN DEB_ARCH=$(echo ${ARCH} | sed 's/x86_64/amd64/g'); dpkg -x src/linux-headers-${KERN_VERSION}-pl_${KERN_VERSION}-pl-1_${DEB_ARCH}.deb .

# Remove broken symlinks
RUN find usr/src/linux-headers-${KERN_VERSION}-pl -xtype l -exec rm {} +

# Remove uneeded files to reduce size
# Keep only:
# - usr/src/linux-headers-x.x.x-pl/include
# - usr/src/linux-headers-x.x.x-pl/arch/${ARCH}
# This reduces the size by a little over 2x.
RUN rm -rf usr/share
RUN find usr/src/linux-headers-${KERN_VERSION}-pl -maxdepth 1 -mindepth 1 ! -name include ! -name arch -type d \
    -exec rm -rf {} +
RUN find usr/src/linux-headers-${KERN_VERSION}-pl/arch -maxdepth 1 -mindepth 1 ! -name $(echo ${ARCH} | sed 's/x86_64/x86/g') -type d -exec rm -rf {} +
RUN tar zcf linux-headers-${ARCH}-${KERN_VERSION}.tar.gz usr

VOLUME /output
CMD ["sh", "-c", "cp linux-headers-*.tar.gz /output/"]
