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

# Invoke something using sudo. This is *just* to force password entry because
# bazel run will not handle password entry correctly.
sudo ls 1> /dev/null

# Disabling the "test sharding strategy" is needed, despite that we are using bazel run here.
bazel run --run_under=sudo --test_sharding_strategy=disabled "$@"
