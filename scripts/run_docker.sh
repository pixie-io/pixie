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

# Some colors to use.
red="\e[0;91m"
green="\e[0;92m"
bold="\e[1m"
resetcolor="\e[0m"

# Docker image information.
docker_image_base=gcr.io/pixie-oss/pixie-dev-public/dev_image_with_extras
version=$(grep DOCKER_IMAGE_TAG "${workspace_root}/docker.properties" | cut -d= -f2)
docker_image_with_tag="${docker_image_base}:${version}"
docker_host_gid=$(getent group docker | awk -F: '{print $3}')

if [[ -z "${docker_host_gid}" ]]; then
    echo "${red}Docker needs to be enabled in order to run the dev container.{$resetcolor}"
    exit 1
fi

shell=/bin/bash
if [[ "${SHELL}" == *"zsh" ]]; then
    shell=/usr/bin/zsh
fi

pushd "${workspace_root}/tools/docker/user_dev_image" > /dev/null
dev_image_name="px_dev_image:${version}"
docker build -q -t "${dev_image_name}" \
       --build-arg BASE_IMAGE="${docker_image_with_tag}" \
       --build-arg USER_NAME="${USER}" \
       --build-arg USER_ID="$(id -u)" \
       --build-arg GROUP_ID="$(id -g)" \
       --build-arg DOCKER_ID="${docker_host_gid}" \
       --build-arg SHELL="${shell}" \
       .
popd > /dev/null

# Check if build buddy cache exists. If so, add the appropriate arguments.
build_buddy_args=()
if grep -q "^build.*remote_cache.*grpc.*9998" /etc/bazelrc; then
    build_buddy_args=(
        "--add-host=$(hostname -f):127.0.0.1"
        "--volume=/etc/bazelrc:/etc/bazelrc")
fi

IFS=' '
# Read the environment variable and set it to an array. This allows
# us to use an array access in the later command.
read -ra PX_RUN_DOCKER_EXTRA_ARGS <<< "${PX_RUN_DOCKER_EXTRA_ARGS}"

configs=(
    # Mount the home directory.
    -v "${HOME}:/home/${USER}"
    # Allow docker to run within the container.
    -v /var/run/docker.sock:/var/run/docker.sock
    # Mount the workspace root within the build directory.
    -v "${workspace_root}:/pl/src/px.dev/pixie"
    # Use the host network so we can easily access build caches, etc.
    "--network=host"
)

exec_cmd=("${shell}")
if [ $# -ne 0 ]; then
  exec_cmd=("${exec_cmd[@]}" "-c" "$*")
fi

echo -e "${bold}Run Parameters:${resetcolor}"
if [ ${#build_buddy_args[@]} -ne 0 ]; then
    echo -e "\t${bold}Build Buddy: \t\t ${green}Enabled${resetcolor}"
else
    echo -e "\t${bold}Build Buddy: \t\t ${red}Disabled${resetcolor}"
fi
echo -e "\t${bold}Shell: \t\t\t ${green}${shell}${resetcolor}"


docker run --rm -it \
  "${configs[@]}" \
  "${build_buddy_args[@]}" \
  "${PX_RUN_DOCKER_EXTRA_ARGS[@]}" \
  "${dev_image_name}" \
  "${exec_cmd[@]}"
