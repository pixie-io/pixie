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

FROM ubuntu:22.04@sha256:67211c14fa74f070d27cc59d69a7fa9aeff8e28ea118ef3babc295a0428a6d21

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing
RUN apt-get install -y ruby ruby-dev rubygems build-essential git wget patchelf
RUN gem install --no-document fpm

WORKDIR /
RUN git clone --depth 1 https://github.com/richfelker/musl-cross-make.git

WORKDIR /musl-cross-make
RUN echo '\n\
TARGET=x86_64-alpine-linux-musl\n\
GCC_VER=11.2.0\n\
OUTPUT=/opt/gcc-musl-11.2.0\n\
MUSL_CONFIG=CFLAGS="-fPIC" CXXFLAGS="-fPIC"\n\
' > config.mak

RUN make -j$(nproc) install

# By default the cross-compiled gcc will link against libc.so instead of what alpine uses.
# Thist patch forces the cross-compiled gcc to produce shared links that appear the same as shared links from a native alpine toolchain.
RUN patchelf --set-soname libc.musl-x86_64.so.1 /opt/gcc-musl-11.2.0/x86_64-alpine-linux-musl/lib/libc.so


WORKDIR /opt
VOLUME /image
ENV deb_name gcc-musl-libs-11.2.0.deb
# At the moment, we only need the libs produced by the gcc cross build. If we need the full toolchain in the future
# the paths at the end of the command should be changed to just `gcc-musl-11.2.0`

CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
  -s dir -t deb -n gcc-musl-libs -v 11.2.0 --prefix=/opt \
  gcc-musl-11.2.0/lib/gcc/x86_64-alpine-linux-musl/11.2.0 \
  gcc-musl-11.2.0/x86_64-alpine-linux-musl/lib \
  gcc-musl-11.2.0/x86_64-alpine-linux-musl/include"]
