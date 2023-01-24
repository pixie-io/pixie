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

set -e

usage() {
  echo "Usage $0:"
  echo "    --llvm_git_repo (-g) <llvm_git_repo_path>"
  echo "    --c_compiler_path (-c) <c_compiler_path>"
  echo "    --cxx_compiler_path (-p) <cxx_compiler_path>"
  echo "    --install_dir (-o) <install_dir>"
  echo "    --build_type (-b) <build_type>"
  echo "    [--target_libcxx_path (-x) <target_libcxx_path>]"
  echo "    [--sysroot (-s) <sysroot_path>]"
  echo "    [--target_arch (-a) <target_arch> The architecture the toolchain will run on]"
  echo "    [--build_with_asan (-d)]"
  echo "    [--build_with_msan (-m)]"
  echo "    [--build_with_tsan (-t)]"
  exit 1;
}

OPTS="$(getopt -o g:c:p:o:b:x:s:a:hdmt --long 'llvm_git_repo:,c_compiler_path:,cxx_compiler_path:,install_dir:,build_type:,target_libcxx_path:,,sysroot:,target_arch:,build_with_asan,build_with_msan,build_with_tsan' -- "$@")"

replace_long_with_short() {
  OPTS="${OPTS/--$1/-$2}"
}
replace_long_with_short "llvm_git_repo" "g"
replace_long_with_short "c_compiler_path" "c"
replace_long_with_short "cxx_compiler_path" "p"
replace_long_with_short "install_dir" "o"
replace_long_with_short "build_type" "b"
replace_long_with_short "target_libcxx_path" "x"
replace_long_with_short "sysroot" "s"
replace_long_with_short "target_arch" "a"
replace_long_with_short "build_with_asan" "d"
replace_long_with_short "build_with_msan" "m"
replace_long_with_short "build_with_tsan" "t"

eval set -- "$OPTS"

llvm_git_path=""
c_compiler_path=""
cxx_compiler_path=""
install_dir=""
build_type=""
sysroot=""
target_arch=""
common_patches=("/patches/zdebug.15.0.6.patch")
extra_patches=()
extra_cmake_options=()

target_cxxflags="-fPIC"
target_cflags="-fPIC"
target_ldflags="-fuse-ld=lld"
target_libcxx_path=""

install_targets=()
is_sanitizer_build=""

with_libcxx() {
  target_libcxx_path="$1"
  target_cxxflags="${target_cxxflags} -stdlib=libc++ -nostdinc++ -isystem ${target_libcxx_path}/include/c++/v1"
  target_ldflags="${target_ldflags} -L${target_libcxx_path}/lib -Wl,-rpath,${target_libcxx_path}/lib"
}

with_asan() {
  extra_cmake_options+=("-DLLVM_USE_SANITIZER=Address;Undefined")
  is_sanitizer_build="true"
}
with_msan() {
  extra_cmake_options+=("-DLLVM_USE_SANITIZER=Memory")
  is_sanitizer_build="true"
}
with_tsan() {
  extra_cmake_options+=("-DLLVM_USE_SANITIZER=Thread")
  is_sanitizer_build="true"
}
with_host_tblgen() {
  host_bin_dir="$(dirname "${c_compiler_path}")"
  extra_cmake_options+=("-DLLVM_TABLEGEN=${host_bin_dir}/llvm-tblgen")
  extra_cmake_options+=("-DCLANG_TABLEGEN=${host_bin_dir}/clang-tblgen")
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "g:c:p:o:b:x:s:a:hdmt" opt; do
    case ${opt} in
      g)
        llvm_git_path=$OPTARG
        ;;
      c)
        c_compiler_path=$OPTARG
        ;;
      p)
        cxx_compiler_path=$OPTARG
        ;;
      o)
        install_dir=$OPTARG
        ;;
      b)
        build_type=$OPTARG
        ;;
      x)
        with_libcxx "$OPTARG"
        ;;
      s)
        sysroot=$OPTARG
        ;;
      a)
        target_arch=$OPTARG
        ;;
      d)
        with_asan
        ;;
      m)
        with_msan
        ;;
      t)
        with_tsan
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

verify_args() {
  missing=""
  if [ -z "${llvm_git_path}" ]; then
    missing="${missing} -g"
  fi
  if [ -z "${c_compiler_path}" ]; then
    missing="${missing} -c"
  fi
  if [ -z "${cxx_compiler_path}" ]; then
    missing="${missing} -p"
  fi
  if [ -z "${install_dir}" ]; then
    missing="${missing} -o"
  fi
  if [ -z "${build_type}" ]; then
    missing="${missing} -b"
  fi

  if [ -n "${missing}" ]; then
    echo "Missing required arguments: ${missing}" 1>&2
    exit 1
  fi
}

parse_args "$@"
verify_args

apply_patches() {
  pushd "${llvm_git_path}" > /dev/null
  for file in "$@"
  do
    git apply "${file}"
  done
  popd > /dev/null
}

reset_git_repo() {
  pushd "${llvm_git_path}" > /dev/null
  git stash
  popd > /dev/null
}

run_llvm_build() {
  local projects_to_build="${projects_to_build}"
  local target_archs="${target_archs}"
  local build_arch="${build_arch:-x86_64}"
  local build_os="${build_os:-linux}"
  local build_abi="${build_abi:-gnu}"
  local target_os="${target_os:-linux}"
  local target_arch="${target_arch:-x86_64}"
  local target_abi="${target_abi:-gnu}"
  local sysroot="${sysroot}"

  default_triple="${target_arch}-${target_os}-${target_abi}"

  reset_git_repo
  apply_patches "${common_patches[@]}"

  if [[ "${#extra_patches[@]}" -gt 0 ]]; then
    apply_patches "${extra_patches[@]}"
  fi

  cmake_dir="$(mktemp -d)"
  pushd "${cmake_dir}" > /dev/null

  host_compiler_bin_path="$(dirname "${c_compiler_path}")"
  export PATH="${host_compiler_bin_path}:${PATH}"
  export CC="${c_compiler_path}"
  export CXX="${cxx_compiler_path}"

  cmake_options=(
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_INSTALL_PREFIX=${install_dir}"
    "-DMSVC=OFF"
    "-DLLVM_BUILD_DOCS=OFF"
    "-DLLVM_INCLUDE_EXAMPLES=OFF"
    "-DLLVM_BUILD_32_BITS=OFF"
    "-DLLVM_TARGETS_TO_BUILD=${target_archs}"
    "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF"
    "-DLLVM_ENABLE_SPHINX=OFF"
    "-DLLVM_ENABLE_DOXYGEN=OFF"
    "-DLLVM_ENABLE_RTTI=ON"
    "-DCLANG_INCLUDE_TESTS=OFF"
    "-DLIBCLANG_BUILD_STATIC=ON"
    "-DLLVM_INCLUDE_TESTS=OFF"
    "-DLLVM_DEFAULT_TARGET_TRIPLE=${default_triple}"
    "-DLLVM_ENABLE_PROJECTS=${projects_to_build}"
    "-DCMAKE_C_FLAGS=${target_cflags}"
    "-DCMAKE_CXX_FLAGS=${target_cxxflags}"
    "-DCMAKE_EXE_LINKER_FLAGS=${target_ldflags}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${target_ldflags}"
    "-DCMAKE_MODULE_LINKER_FLAGS=${target_ldflags}"
  )

  if [[ "${target_arch}" != "${build_arch}" ]] || [ -n "${sysroot}" ]; then
    if [ -z "${sysroot}" ]; then
      echo "Must specify sysroot to cross compile llvm"
      exit 1
    fi
    cmake_options+=("-DCMAKE_SYSTEM_NAME=${target_os^}")
    cmake_options+=("-DCMAKE_SYSTEM_PROCESSOR=${target_arch}")
    cmake_options+=("-DCMAKE_SYSROOT=${sysroot}")
    cmake_options+=("-DCMAKE_C_COMPILER=${c_compiler_path}")
    cmake_options+=("-DCMAKE_CXX_COMPILER=${cxx_compiler_path}")
    cmake_options+=("-DCMAKE_ASM_COMPILER=${c_compiler_path}")
    cmake_options+=("-DCMAKE_CXX_COMPILER_TARGET=${target_arch}-${target_os}-${target_abi}")
    cmake_options+=("-DCMAKE_C_COMPILER_TARGET=${target_arch}-${target_os}-${target_abi}")
    cmake_options+=("-DLLVM_HOST_TRIPLE=${build_arch}-${build_os}-${build_abi}")
    cmake_options+=("-DLLVM_USE_HOST_TOOLS=True")
  elif [[ "${is_sanitizer_build}" = "true" ]]; then
    # If we're doing a sanitizer build, we make cmake think its cross-compiling
    # so that llvm doesn't attempt to run some unnecessary tools built with sanitizer instrumentation.
    cmake_options+=("-DCMAKE_SYSTEM_NAME=${target_os^}")
  fi
  cmake_options+=("${extra_cmake_options[@]}")

  cmake -G Ninja "${cmake_options[@]}" "${llvm_git_path}"/llvm

  if [[ "${#install_targets[@]}" -eq 0 ]]; then
    ninja install
  else
    ninja "${install_targets[@]}"
    ninja "${install_targets[@]/#/install-}"
  fi

  popd > /dev/null
  rm -rf "${cmake_dir}"
}

build_full_clang() {
  projects_to_build="clang;clang-tools-extra;polly;llvm;lld;compiler-rt"
  target_archs="X86;AArch64"
  extra_cmake_options+=("-DLLVM_STATIC_LINK_CXX_STDLIB=ON")
  extra_patches+=("/patches/install_tblgen.15.0.6.patch")
  run_llvm_build
}

build_libcxx() {
  projects_to_build="libcxx;libcxxabi"
  target_archs="BPF;X86;AArch64"
  install_targets=("cxx" "cxxabi")
  run_llvm_build
}

build_llvm_libs() {
  projects_to_build="clang;llvm"
  target_archs="BPF;X86;AArch64"
  extra_cmake_options+=("-DLLVM_BUILD_TOOLS=OFF")
  with_host_tblgen
  run_llvm_build
}

build_minimal_clang() {
  projects_to_build="clang;clang-tools-extra;llvm;lld;compiler-rt"
  target_archs="BPF;X86;AArch64"
  extra_cmake_options+=(
    "-DLLVM_ENABLE_PLUGINS=OFF"
    "-DLLVM_STATIC_LINK_CXX_STDLIB=ON"
    "-DLLVM_TOOL_LTO_BUILD=OFF"
    "-DLLVM_BUILD_LLVM_DYLIB=OFF"
    "-DLLVM_BUILD_SHARED_LIBS=OFF"
    "-DCLANG_PLUGIN_SUPPORT=OFF")
  extra_patches+=(
    "/patches/static_tinfo.15.0.6.patch"
    "/patches/disable_shared_libclang.15.0.6.patch")
  target_ldflags="${target_ldflags} -static-libgcc"
  with_host_tblgen
  run_llvm_build
}

case "${build_type}" in
  full_clang)
    build_full_clang
    ;;
  libcxx)
    if [ -n "${target_libcxx_path}" ]; then
      echo "Invalid arguments: don't specify -x <target_libcxx_path> when building libcxx" 1>&2;
      exit 1
    fi
    build_libcxx
    ;;
  llvm_libs)
    build_llvm_libs
    ;;
  minimal_clang)
    build_minimal_clang
    ;;
  *)
    echo "-b <build_type> must be one of full_clang, minimal_clang, libcxx, llvm_libs " 1>&2;
    exit 1
    ;;
esac
