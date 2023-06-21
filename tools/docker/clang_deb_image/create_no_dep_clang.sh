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

set -x
set -e

base_dir="$1"
tgz_file="$2"

min_dir=$(mktemp -d)

clang_exec_list=(clang-cpp clang-15 clang clang++
		 ld.lld lld
		 llvm-ar llvm-as llvm-cov llvm-dwp llvm-nm llvm-objcopy llvm-objdump llvm-profdata llvm-strip)

mkdir -p "${min_dir}/bin"

for f in "${clang_exec_list[@]}"; do
  cp -P "${base_dir}/bin/$f" "${min_dir}/bin/$f"
done

pushd "${base_dir}"

cp --parents -R lib/clang "${min_dir}"
cp --parents lib/clang/*/lib/linux/*san* "${min_dir}"

tar -czf "${tgz_file}" -C "${min_dir}" .

popd
