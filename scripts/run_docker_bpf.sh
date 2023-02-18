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

# Script Usage:
# run_docker_bpf.sh <cmd to run in docker container>

script_dir="$(dirname "$0")"
run_docker_script="${script_dir}/run_docker.sh"

# This adds all the privileged flags necessary to run BPF code.
bpf_flags=(--privileged \
  -v /:/host \
  -v /sys:/sys \
  -v /var/lib/docker:/var/lib/docker \
  "--pid=host" \
  --cgroupns host \
  --env "PL_HOST_PATH=/host")

# Carry along any previously set PX_RUN_DOCKER_EXTRA_ARGS but prepend the bpf_flags to them.
export PX_RUN_DOCKER_EXTRA_ARGS="${bpf_flags[*]} ${PX_RUN_DOCKER_EXTRA_ARGS}"

"${run_docker_script}" "$@"
