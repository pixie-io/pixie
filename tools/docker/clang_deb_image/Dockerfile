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
  clang-14 \
  lld-14

ENV CC=clang-14
ENV CXX=clang++-14

WORKDIR /llvm_all
RUN git clone --branch llvmorg-15.0.6 --depth 1 https://github.com/llvm/llvm-project.git

ENV LLVM_GIT_PATH="/llvm_all/llvm-project"

RUN git config --global user.email "build@pixielabs.ai" && \
  git config --global user.name "robot"

COPY patches/build/* /patches/
COPY build_llvm.sh /scripts/build_llvm.sh

# We build a few different versions of LLVM/Clang for our various different build setups.
#   1. The host build to make sure we can build all the other versions consitently.
#   2. Libcxx only that we can use standalone.
#   3. LLVM libs with libcxx.
#   4. LLVM libs wiht libstdc++ (system version of libstdc++).
#   5. The minimal version of clang that we can distribute as a dependency to build the rest of Pixie.
# We have to do steps 2-4 using each "sysroot", currently we support the following "sysroots":
#   - The host ubuntu:18.04 root (i.e. no sysroot).
#   - x86_64 glibc2.36 sysroot.
#   - aarch64 glibc2.36 sysroot.

#--------------------------------------------------------
# 1. Host LLVM/Clang
#--------------------------------------------------------
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "$(readlink -f "$(which clang-14)")" \
  --cxx_compiler_path "$(which clang++-14)" \
  --install_dir /opt/clang-15.0 \
  --build_type full_clang

WORKDIR /sysroots/archives
# Download build sysroots.
RUN curl -L https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-amd64-build.tar.gz -o sysroot-amd64-build.tar.gz && \
  mkdir -p /sysroots/x86_64 && \
  tar -C /sysroots/x86_64 -xzf sysroot-amd64-build.tar.gz

RUN curl -L https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-arm64-build.tar.gz -o sysroot-arm64-build.tar.gz && \
  mkdir -p /sysroots/aarch64 && \
  tar -C /sysroots/aarch64 -xzf sysroot-arm64-build.tar.gz

WORKDIR /llvm_all

ENV CLANG="/opt/clang-15.0/bin/clang"
ENV CLANGPP="/opt/clang-15.0/bin/clang++"

#--------------------------------------------------------
# 2. Libcxx
#--------------------------------------------------------
# No sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/libcxx-15.0 \
  --build_type libcxx

# x86_64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/libcxx-15.0-x86_64-sysroot \
  --build_type libcxx \
  --sysroot /sysroots/x86_64 \
  --target_arch x86_64

# aarch64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/libcxx-15.0-aarch64-sysroot \
  --build_type libcxx \
  --sysroot /sysroots/aarch64 \
  --target_arch aarch64


#--------------------------------------------------------
# 3. LLVM Libs with Libcxx
#--------------------------------------------------------


# No sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0

# No sysroot with ASAN instrumentation.
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx-asan \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0 \
  --build_with_asan

# No sysroot with MSAN instrumentation.
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx-msan \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0 \
  --build_with_msan

# No sysroot with TSAN instrumentation.
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx-tsan \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0 \
  --build_with_tsan

# x86_64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx-x86_64-sysroot \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0-x86_64-sysroot \
  --sysroot /sysroots/x86_64 \
  --target_arch x86_64

# aarch64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libcxx-aarch64-sysroot \
  --build_type llvm_libs \
  --target_libcxx_path /opt/libcxx-15.0-aarch64-sysroot \
  --sysroot /sysroots/aarch64 \
  --target_arch aarch64

#--------------------------------------------------------
# 4. LLVM Libs with libstdcxx
#--------------------------------------------------------

# No sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libstdc++ \
  --build_type llvm_libs

# x86_64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libstdc++-x86_64-sysroot \
  --build_type llvm_libs \
  --sysroot /sysroots/x86_64 \
  --target_arch x86_64

# aarch64 sysroot
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/llvm-15.0-libstdc++-aarch64-sysroot \
  --build_type llvm_libs \
  --sysroot /sysroots/aarch64 \
  --target_arch aarch64

#--------------------------------------------------------
# 5. Minimal clang
#--------------------------------------------------------
RUN /scripts/build_llvm.sh \
  --llvm_git_repo "${LLVM_GIT_PATH}" \
  --c_compiler_path "${CLANG}" \
  --cxx_compiler_path "${CLANGPP}" \
  --install_dir /opt/clang-15.0-nodeps-build \
  --build_type minimal_clang

COPY create_no_dep_clang.sh /opt

WORKDIR /opt
RUN ./create_no_dep_clang.sh "/opt/clang-15.0-nodeps-build" "/opt/clang-15.0-min.tar.gz"


WORKDIR /opt
VOLUME /image
COPY create_packages.sh /opt
COPY patches/llvm_cmake.patch /patches/llvm_cmake.patch

CMD ["/opt/create_packages.sh"]
