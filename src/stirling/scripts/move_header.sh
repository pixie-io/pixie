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

if [[ $# -lt 2 ]]; then
  echo "$0 <header_file_path> <target_directory>"
  echo "Move a single header file to another directory. Also tries to move the .cc and _test.cc."
  exit 1
fi

hdr="$1"
target_directory="$2"

library_prefix="${hdr%.*}"
echo "library_prefix: ${library_prefix} ..."
hdr="${library_prefix}.h"
src="${library_prefix}.cc"
tst="${library_prefix}_test.cc"

if [[ -f "${hdr}" ]]; then
  mv "${hdr}" "${target_directory}"
fi

if [[ -f "${src}" ]]; then
  mv "${src}" "${target_directory}"
fi

if [[ -f "${tst}" ]]; then
  mv "${tst}" "${target_directory}"
fi

target_hdr="${target_directory}/$(basename "${hdr}")"
sed_script="s|${hdr}|${target_hdr}|"
echo "Use \"sed -i '${sed_script}' $(grep "${hdr}" src -Rl)\""

# We want grep output to be interpreted as multiple file paths instead of one string
# shellcheck disable=SC2046
sed -i "${sed_script}" $(grep "${hdr}" src -Rl)
