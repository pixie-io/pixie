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

denylist=(
  # container_run_and_* are banned because they bypass our licensing rules, and slow down builds.
  "container_run_and_commit"
  "container_run_and_extract"
  "container_run_and_commit_layer"
)

for entry in "${denylist[@]}"
do
  # TODO(james): this will match comments.
  grep -nHo "$entry" "$1" | sed 's/:[^:]*$/:error:'"${entry}"' is on bazel denylist and should not be used/g';
done
