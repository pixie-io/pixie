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

BUILDDIR=/tmp/blahdyblah
PKGDIR=/tmp/out
workspace=$(bazel info workspace 2> /dev/null)

# Clean up build dir
rm -rf "${BUILDDIR}"
rm -rf "${PKGDIR}"
# Go to TOT
cd "${workspace}"
bazel build //src/api/python:build_pip_package

bazel-bin/src/api/python/build_pip_package --src "${BUILDDIR}" --dest "${PKGDIR}"


# uncomment to upload to main pypi
# twine upload ${PKGDIR}/*

# Upload to test pypi
twine upload --repository testpypi ${PKGDIR}/*

