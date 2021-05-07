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
# run_docker.sh <cmd to run in docker container>

script_dir="$(dirname "$0")"
workspace_root=$(realpath "${script_dir}/../")

# Docker image information.
docker_image_base=gcr.io/pixie-oss/pixie-dev-public/dev_image_with_extras
version=$(grep DOCKER_IMAGE_TAG "${workspace_root}/docker.properties" | cut -d= -f2)
docker_image_with_tag="${docker_image_base}:${version}"

IFS=' '
# Read the environment variable and set it to an array. This allows
# us to use an array access in the later command.
read -ra PX_RUN_DOCKER_EXTRA_ARGS <<< "${PX_RUN_DOCKER_EXTRA_ARGS}"

configs=(-v "$HOME/.config:/root/.config" \
  -v "$HOME/.ssh:/root/.ssh" \
  -v "$HOME/.kube:/root/.kube" \
  -v "$HOME/.gitconfig:/root/.gitconfig" \
  -v "$HOME/.arcrc:/root/.arcrc")

exec_cmd=("/usr/bin/bash")
if [ $# -ne 0 ]; then
  exec_cmd=("${exec_cmd[@]}" "-c" "$*")
fi

docker run --rm -it \
  "${configs[@]}" \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v "${workspace_root}:/pl/src/px.dev/pixie" \
  "${PX_RUN_DOCKER_EXTRA_ARGS[@]}" \
  "${docker_image_with_tag}" \
  "${exec_cmd[@]}"
