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

FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing
RUN apt-get install -y ruby ruby-dev rubygems build-essential
RUN gem install --no-document fpm

RUN apt-get install -y bison build-essential flex git libedit-dev lsb \
  python3 python3-distutils swig libncurses5-dev zlib1g-dev libelf-dev subversion gpg \
  gcc-multilib software-properties-common ninja-build

# We need a newer version of cmake to compile the latest llvm.
RUN curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

# We need a newer version of clang to compile the latest llvm.
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" && \
  add-apt-repository ppa:ubuntu-toolchain-r/test && \
  add-apt-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-14 main"

RUN apt update
RUN apt-get install -y kitware-archive-keyring \
  cmake \
  clang-14

ENV CC=clang-14
ENV CXX=clang++-14

# We build a few different versions of LLVM/Clang for our various different build setups.
#   1. The host build to make sure we can build all the other versions consitently.
#   2. Libcxx only that we can use standalone.
#   3. LLVM libs with libcxx.
#   4. LLVM libs wiht libstdc++ (system version of libstdc++).
#   5  LLVM/Clang with libc++ that we can package into a deb.
#   6. The minimal version of clang that we can distribute as a dependency to build the rest of Pixie.

WORKDIR /llvm_all
RUN git clone --branch llvmorg-15.0.6 --depth 1 https://github.com/llvm/llvm-project.git
RUN cp -a llvm-project llvm-project-original

COPY zdebug.15.0.6.patch zdebug.patch

RUN cd llvm-project && git apply ../zdebug.patch

#--------------------------------------------------------
# 1. Host LLVM/Clang
#--------------------------------------------------------
WORKDIR /llvm_all/build
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-15.0 \
    -DLLVM_ENABLE_PLUGINS=OFF \
    -DMSVC=OFF \
    -DLLVM_BUILD_DOCS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_32_BITS=OFF \
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLLVM_ENABLE_SPHINX=OFF \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_STATIC_LINK_CXX_STDLIB=ON \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;polly;llvm;lld;compiler-rt" \
    ../llvm-project/llvm

RUN ninja -j $(nproc)
RUN ninja install

#--------------------------------------------------------
# 2. Libcxx
#--------------------------------------------------------
ENV PATH=/opt/clang-15.0/bin:${PATH}
ENV CC=clang
ENV CXX=clang++

WORKDIR /llvm_all/build_libcxx_only
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/libcxx-15.0 \
    -DMSVC=OFF \
    -DLLVM_BUILD_DOCS=OFF \
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
    -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi" \
    ../llvm-project/llvm


RUN ninja -j $(nproc) cxx cxxabi
RUN ninja install-cxx install-cxxabi

#######################################################
# This builds all the clang/LLVM libs that we can package for our code in a source file.
#
# Since we link Clang/LLVM with our own source code we
# need it to be built with the same underlying C++
# library.
#######################################################


#--------------------------------------------------------
# 3. LLVM Libs with Libcxx
#--------------------------------------------------------
ENV PATH=/opt/clang-15.0/bin:${PATH}
ENV CC=clang
ENV CXX=clang++
ENV CXXFLAGS="-fPIC -stdlib=libc++ -L/opt/libcxx-15.0/lib -isystem /opt/libcxx-15.0/include/c++/v1 -Wl,-rpath,/opt/libcxx-15.0/lib"

WORKDIR /llvm_all/build_llvm_libcxx
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/llvm-15.0-libcxx \
    -DMSVC=OFF \
    -DLLVM_BUILD_DOCS=OFF \
    -DLLVM_BUILD_TOOLS=OFF \
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
    -DLLVM_ENABLE_PROJECTS="clang;llvm" \
  ../llvm-project/llvm

RUN ninja -j $(nproc)
RUN ninja install

#--------------------------------------------------------
# 4. LLVM Libs with libstdcxx
#--------------------------------------------------------
ENV PATH=/opt/clang-15.0/bin:${PATH}
ENV CC=clang
ENV CXX=clang++
ENV CXXFLAGS=

WORKDIR /llvm_all/build_llvm_libstdc++
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/llvm-15.0-libstdc++ \
    -DMSVC=OFF \
    -DLLVM_BUILD_DOCS=OFF \
    -DLLVM_BUILD_TOOLS=OFF \
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
    -DLLVM_ENABLE_PROJECTS="clang;llvm" \
  ../llvm-project/llvm

RUN ninja -j $(nproc)
RUN ninja install

WORKDIR /llvm_all
COPY static_tinfo.15.0.6.patch tinfo.patch
COPY disable_shared_libclang.15.0.6.patch disable_shared_libclang.patch
RUN cd llvm-project && git apply ../disable_shared_libclang.patch && git apply ../tinfo.patch

ENV CXXFLAGS="-fPIC"
ENV LDFLAGS="-static-libgcc"

WORKDIR /llvm_all/build_clang_nodeps
RUN triple=$(gcc -v 2>&1 | grep "^Target:" | cut -d ' ' -f 2) && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-15.0-nodeps-build \
    -DMSVC=OFF \
    -DLLVM_BUILD_DOCS=OFF \
    -DLLVM_BUILD_TOOLS=ON \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_32_BITS=OFF \
    -DLLVM_TARGETS_TO_BUILD="BPF;X86;AArch64" \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLLVM_ENABLE_SPHINX=OFF \
    -DLLVM_ENABLE_DOXYGEN=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLIBCLANG_BUILD_STATIC=ON \
    -DLLVM_STATIC_LINK_CXX_STDLIB=ON\
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_TOOL_LTO_BUILD=OFF \
    -DLLVM_BUILD_LLVM_DYLIB=OFF \
    -DLLVM_BUILD_SHARED_LIBS=OFF \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${triple} \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;llvm;lld;compiler-rt" \
    -DCLANG_PLUGIN_SUPPORT=OFF \
  ../llvm-project/llvm

RUN ninja -j $(nproc)
RUN ninja install

COPY create_no_dep_clang.sh /opt

WORKDIR /opt
RUN ./create_no_dep_clang.sh "/opt/clang-15.0-nodeps-build" "/opt/clang-15.0-min.tar.gz"


WORKDIR /opt
VOLUME /image
COPY create_packages.sh /opt
COPY llvm_cmake.patch /opt

CMD ["/opt/create_packages.sh"]
