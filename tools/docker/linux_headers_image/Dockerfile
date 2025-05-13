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

ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG KERN_VERSION
ARG ARCH
ARG CROSS_COMPILE="false"

# Install required packages
ENV DEBIAN_FRONTEND=noninteractive
ENV KERN_VERSION=${KERN_VERSION}
ENV ARCH=${ARCH}
ENV CROSS_COMPILE=${CROSS_COMPILE}

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
  debhelper \
  python3

WORKDIR /configs
ADD x86_64_config /configs/x86_64
ADD arm64_config /configs/arm64

WORKDIR /px

ADD build_linux_headers.sh /px/build_linux_headers.sh

RUN chmod +x /px/build_linux_headers.sh

VOLUME /output
CMD ["sh", "-c", "/px/build_linux_headers.sh /px ${KERN_VERSION} ${ARCH} ${CROSS_COMPILE} /output"]
