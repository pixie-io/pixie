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

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing
RUN apt-get install -y ruby ruby-dev rubygems build-essential
RUN gem install --no-document fpm

RUN apt-get install -y bison build-essential cmake flex git libedit-dev \
  clang libclang-dev llvm llvm-dev \
  python3 python3-distutils swig libncurses5-dev zlib1g-dev libelf-dev subversion \
  gcc-multilib

ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all
RUN git clone --branch llvmorg-14.0.4 --depth 1 https://github.com/llvm/llvm-project.git

WORKDIR /llvm_all/build
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-14.0 \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_32_BITS=OFF \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86;AArch64" \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLLVM_ENABLE_SPHINX=OFF \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;polly;lld;lldb;openmp;libcxx;libcxxabi;compiler-rt" \
    ../llvm-project/llvm


RUN make -j $(nproc)
RUN make -j $(nproc) runtimes
RUN make install

## We need libclang.a, but the clang build system excludes it during make install.
#RUN cp -a /llvm_all/build/lib/libclang.a /opt/clang-14.0/lib


#######################################################
# This installs Clang/LLVM with libc++.
#
# Since we link Clang/LLVM with our own source code we
# need it to be built with the same underlying C++
# library.
#######################################################
ENV PATH=/opt/clang-14.0/bin:${PATH}
ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all/build_libc++
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
    CXXFLAGS="-stdlib=libc++" \
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=/opt/clang-14.0-libc++ \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_32_BITS=OFF \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86;AArch64" \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLLVM_ENABLE_SPHINX=OFF \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;polly;lld;lldb;openmp;libcxx;libcxxabi;compiler-rt" \
  ../llvm-project/llvm

RUN make -j $(nproc)
RUN make -j $(nproc) runtimes
RUN make install

# LSIF is broken with clang-14
# # Add clang-lsif for our image. This needs to be patched for the version of clang being used.
# WORKDIR /tmp
# RUN git clone https://github.com/pixie-io/lsif-clang.git
#
# WORKDIR /tmp/lsif-clang
# RUN git checkout llvm-11
# RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/clang-14.0
# RUN make -C build -j $(nproc)
# RUN make -C build install

WORKDIR /opt
VOLUME /image
ENV deb_name clang-14.0-pl1.deb
CMD ["sh", "-c",  "fpm -p /image/${deb_name} \
        -s dir -t deb -n clang-14.0 -v 14.0-pl1 --prefix /opt clang-14.0 clang-14.0-libc++"]
