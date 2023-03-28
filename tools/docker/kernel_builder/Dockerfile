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

FROM debian:bullseye@sha256:7b991788987ad860810df60927e1adbaf8e156520177bd4db82409f81dd3b721

ARG KERNEL_VERSION
ENV KERNEL_VERSION=${KERNEL_VERSION}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing \
    && apt-get install -y build-essential git gcc-multilib wget flex bison bc libelf-dev zstd kmod rsync python3

WORKDIR /work
COPY build_kernel.sh /work/build_kernel.sh
COPY kernel.config /work/kernel.config
RUN ./build_kernel.sh -v "${KERNEL_VERSION}" -c kernel.config -w /work

VOLUME /output
CMD ["sh", "-c", "cp /work/pkg.tar.gz /output/linux-build-${KERNEL_VERSION}.tar.gz"]
