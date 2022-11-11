#!/bin/bash -e

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

RELEASE_VERSION=0.5.2

workspace=$(git rev-parse --show-toplevel)
pushd "${workspace}" || exit

if [[ ! -d bazel-compilation-database-${RELEASE_VERSION} ]]; then
  curl -L https://github.com/grailbio/bazel-compilation-database/archive/${RELEASE_VERSION}.tar.gz | tar -xz
  pushd bazel-compilation-database-${RELEASE_VERSION} || exit
  patch -p1 < "${workspace}/third_party/bazel_compilation_database.patch"
  popd || exit
fi

bazel-compilation-database-${RELEASE_VERSION}/generate.py -q "$1"

popd || return
