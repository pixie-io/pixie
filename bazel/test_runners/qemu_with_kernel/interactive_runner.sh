#!/usr/bin/env -S -i /bin/bash

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

# shellcheck shell=bash

execroot="$(realpath "${PWD%%/execroot/px/*}/execroot")"
# Make OLDPWD look like what bazel's test setup does.
export OLDPWD="${execroot}/px"
export RUNFILES_DIR="$PWD"
export INTERACTIVE_MODE="true"

# shellcheck disable=SC2288
%qemu_runner_path% %test_path%
