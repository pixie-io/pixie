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
set -x

CLANG_TAG="${CLANG_VERSION}-${CLANG_SUFFIX}"
CLANG_DEB_IMAGE_NAME="clang-${CLANG_TAG}.deb"
CLANG_LINTER_DEB_IMAGE_NAME="clang-linters-${CLANG_TAG}.deb"

MIN_CLANG_TAR_FILE_NAME="clang-min-${CLANG_TAG}.tar.gz"
CLANG_NO_DEP_LOCATION="/opt/clang-${CLANG_VERSION}-nodeps-build"

tar_libcxx() {
  sysroot="$1"
  if [ -n "${sysroot}" ]; then
    sysroot="-${sysroot}-sysroot"
  fi
  dir="/opt/libcxx-${CLANG_VERSION}${sysroot}"
  tar_file="/image/libcxx-${CLANG_TAG}${sysroot}.tar.gz"
  pushd "${dir}" > /dev/null
  tar -czf "${tar_file}" lib include
  popd > /dev/null
}

tar_libcxx ""
tar_libcxx "x86_64"
tar_libcxx "aarch64"

tar_args=('--exclude=*.so'
	  '--exclude=*.so.*')


patch_llvm_cmake() {
  patch -p1 < /patches/llvm_cmake.patch
}

tar_llvm_libs() {
  libcxx="$1"
  sysroot="$2"
  sanitizer="$3"
  if [ -n "${sysroot}" ]; then
    sysroot="-${sysroot}-sysroot"
  fi
  if [ -n "${sanitizer}" ]; then
    sanitizer="-${sanitizer}"
  fi
  dir="/opt/llvm-${CLANG_VERSION}-${libcxx}${sysroot}${sanitizer}"
  tar_file="/image/llvm-${CLANG_TAG}-${libcxx}${sysroot}${sanitizer}.tar.gz"

  pushd "${dir}" > /dev/null
  patch_llvm_cmake
  tar "${tar_args[@]}" -czf "${tar_file}" lib include
  popd > /dev/null
}

tar_llvm_libs "libcxx" ""
tar_llvm_libs "libcxx" "" "asan"
tar_llvm_libs "libcxx" "" "msan"
tar_llvm_libs "libcxx" "" "tsan"
tar_llvm_libs "libcxx" "x86_64"
tar_llvm_libs "libcxx" "aarch64"

tar_llvm_libs "libstdc++" ""
tar_llvm_libs "libstdc++" "x86_64"
tar_llvm_libs "libstdc++" "aarch64"

# Create the make deb file hosting clang.
fpm -p "/image/${CLANG_DEB_IMAGE_NAME}" \
    -s dir \
    -t deb \
    -n "clang-${CLANG_VERSION}" \
    -v "${CLANG_TAG}" \
    --prefix /opt/px_dev/tools "clang-${CLANG_VERSION}"

tmpdir=$(mktemp -d)
cp -a "${CLANG_NO_DEP_LOCATION}"/bin/clang-format "${tmpdir}"
cp -a "${CLANG_NO_DEP_LOCATION}"/bin/clang-tidy "${tmpdir}"
cp -a "${CLANG_NO_DEP_LOCATION}"/share/clang/clang-tidy-diff.py "${tmpdir}"
cp -a "${CLANG_NO_DEP_LOCATION}"/share/clang/clang-format-diff.py "${tmpdir}"

pushd "${tmpdir}"

fpm -p "/image/${CLANG_LINTER_DEB_IMAGE_NAME}" \
    -s dir \
    -t deb \
    -n "clang-linters-${CLANG_VERSION}" \
    -v "${CLANG_TAG}" \
    --prefix /opt/px_dev/bin .

popd

cp "/opt/clang-15.0-min.tar.gz" "/image/${MIN_CLANG_TAR_FILE_NAME}"
