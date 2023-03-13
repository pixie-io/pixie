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

output_dir="$(realpath "$1")"
docker_image_tag="$2"

architectures=("amd64" "arm64")
variants=("runtime" "build" "test" "debug")

# shellcheck disable=SC2034
files_runtime=("/package_groups/runtime.yaml")
# shellcheck disable=SC2034
files_build=("/package_groups/runtime.yaml" "/package_groups/build.yaml")
# shellcheck disable=SC2034
files_test=("/package_groups/runtime.yaml" "/package_groups/build.yaml" "/package_groups/test.yaml")
# shellcheck disable=SC2034
files_debug=("/package_groups/runtime.yaml" "/package_groups/build.yaml" "/package_groups/test.yaml" "/package_groups/debug.yaml")

pkgdb_dir="$(mktemp -d)"

download_package_index() {
  arch="$1"
  curl -fL "http://ftp.debian.org/debian/dists/bookworm/main/binary-${arch}/Packages.xz" | \
    xz --decompress > "${pkgdb_dir}/${arch}"
}

build_sysroot() {
  arch="$1"
  variant="$2"
  # shellcheck disable=SC1087
  file_varname="files_$variant[@]"
  package_files=( "${!file_varname}" )
  docker run -it -v "${output_dir}":/build -v "${pkgdb_dir}":/pkgdb "${docker_image_tag}" "/pkgdb/${arch}" "/build/sysroot-${arch}-${variant}.tar.gz" "${package_files[@]}"
}

for arch in "${architectures[@]}"
do
  download_package_index "${arch}"
  for variant in "${variants[@]}"
  do
    echo "Building ${output_dir}/sysroot-${arch}-${variant}.tar.gz"
    build_sysroot "${arch}" "${variant}"
  done
done

rm -rf "${pkgdb_dir:?}"
