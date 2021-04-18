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

builddir=$(dirname "$1")

bad_targets=$(bazel query "$builddir/... except attr('tags', 'manual', $builddir/...)" --noshow_progress 2>/dev/null)

for target in $bad_targets; do
  echo "$1: Experimental build target $target should be marked with tag \"manual\""
done