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

FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing
RUN apt-get install -y ruby ruby-dev rubygems build-essential
RUN gem install --no-document fpm

RUN apt-get install -y bison build-essential cmake flex git libedit-dev \
  clang libclang-dev llvm llvm-dev\
  python3 python3-distutils swig libncurses5-dev zlib1g-dev libelf-dev subversion

ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all
RUN git clone --branch llvmorg-13.0.0 --depth 1 https://github.com/llvm/llvm-project.git

WORKDIR /llvm_all/build
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-13.0 \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86" \
    -DLLVM_ENABLE_SPHINX=Off \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;clangd;compiler-rt;libcxx;libcxxabi;polly;lld;openmp;libunwind" \
    ../llvm-project/llvm

RUN make -j $(nproc)
RUN make install

# We need libclang.a, but the clang build system excludes it during make install.
RUN cp -a /llvm_all/build/lib/libclang.a /opt/clang-13.0/lib

#######################################################
# This installs Clang/LLVM with libc++.
#
# Since we link Clang/LLVM with our own source code we
# need it to be built with the same underlying C++
# library.
#######################################################
WORKDIR /llvm_libc++
RUN git clone https://github.com/pixie-io/install-clang

WORKDIR /llvm_libc++/install-clang
RUN git fetch
RUN git checkout 978976c108249f542c189b7afa14a5a8c0204a3a
RUN ./install-clang -j $(nproc) -A /opt/clang-13.0-libc++ -C
RUN rm -rf /opt/clang-13.0-libc++/src

# TODO(zasgar): MSAN version. It's currently failing to build because
# of an MSAN failure inside of LLVM.
#COPY msan.patch msan.patch
#RUN patch -p1 < msan.patch
#RUN ./install-clang -j $(nproc) -A /opt/clang-13.0-libc++-msan -C

# Add clang-lsif for our image. This needs to be patched for the version of clang being used.
WORKDIR /tmp
RUN git clone https://github.com/pixie-io/lsif-clang.git

WORKDIR /tmp/lsif-clang
RUN git checkout llvm-11
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/clang-13.0
RUN make -C build -j $(nproc)
RUN make -C build install

WORKDIR /opt
VOLUME /image
ENV deb_name clang-13.0-pl1.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-13.0 -v 13.0-pl1 --prefix /opt clang-13.0 clang-13.0-libc++"]
