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
docker_image_base=ghcr.io/pixie-io/dev_image_with_extras
os="$(uname)"
version=$(grep DOCKER_IMAGE_TAG "${workspace_root}/docker.properties" | cut -d= -f2)
digest=$(grep DEV_IMAGE_WITH_EXTRAS_DIGEST "${workspace_root}/docker.properties" | cut -d= -f2)
docker_image_with_tag="${docker_image_base}:${version}"

# Skip checking docker group for Mac because group is not required for docker-machine in Mac.
if [[ "${os}" != "Darwin" ]]; then
  docker_host_gid=$(getent group docker | awk -F: '{print $3}')
  if [[ -z "${docker_host_gid}" ]]; then
      echo "${red}Docker needs to be enabled in order to run the dev container.{$resetcolor}"
      exit 1
  fi
else
  # Set PX_DOCKER_RUN_AS_ROOT to True for Mac
  PX_DOCKER_RUN_AS_ROOT=True
fi

shell=/bin/bash
if [[ "${SHELL}" == *"zsh" ]]; then
  shell=/usr/bin/zsh
fi

if [[ "${PX_DOCKER_RUN_AS_ROOT}" != "True" ]]; then
  pushd "${workspace_root}/tools/docker/user_dev_image" > /dev/null
  dev_image_name="px_dev_image:${version}"
  docker build -q -t "${dev_image_name}" \
    --build-arg BASE_IMAGE="${docker_image_with_tag}" \
    --build-arg BASE_IMAGE_DIGEST="${digest}" \
    --build-arg USER_NAME="${USER}" \
    --build-arg USER_ID="$(id -u)" \
    --build-arg GROUP_ID="$(id -g)" \
    --build-arg DOCKER_ID="${docker_host_gid}" \
    --build-arg SHELL="${shell}" \
    .
  popd > /dev/null
else
  dev_image_name="${docker_image_with_tag}@sha256:${digest}"
fi

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

# We can't tell IFS to ignore spaces in quotes so here is a very janky
# parser to recombine things that were quoted. :(
# This should probably not be in bash!
CLEANED_ARGS=()
CURRENT=""
for arg in "${PX_RUN_DOCKER_EXTRA_ARGS[@]}"; do
  if [[ $CURRENT ]]; then
    CURRENT="${CURRENT} ${arg}"
    CURRENT="${CURRENT//\'/}"
    if [[ $arg = *"'" ]]; then
        CLEANED_ARGS+=("${CURRENT}")
        CURRENT=""
    fi
  elif [[ $arg = *"'"* && $arg != *"'"*"'" ]]; then
    CURRENT="${arg}"
  else
    CLEANED_ARGS+=("${arg//\'/}")
  fi
done

configs=(
  # Mount shm from the host, otherwise docker will restrict to 64MB by default.
  -v /dev/shm:/dev/shm
  # Mount the home directory.
  -v "${HOME}:/home/${USER}"
  # Allow docker to run within the container.
  -v /var/run/docker.sock:/var/run/docker.sock
  # Mount the workspace root within the build directory.
  -v "${workspace_root}:/px/src/px.dev/pixie"
  # Use the host network so we can easily access build caches, etc.
  "--network=host"
)

if command -v px > /dev/null; then
  # If "px" exists, mount it into the container (in a well known path).
  px_path=$(command -v px)
  configs+=(-v "${px_path}:/bin/px")
fi

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

container_args=(
  --rm
  --hostname "px-dev-docker-$(hostname)"
)

if [ -t 1 ]; then
  # stdout (file descriptor 1) is affilated with a terminal.
  # It is ok to use the '-it' arg. for docker run.
  container_args+=("-it")
else
  # Use of 'docker run -it <...>' from a system call from inside of go causes docker to
  # complain that a TTY does not exist. This branch, where stdout is not affilated with a terminal
  # does *not* incorporate arg. '-it' for docker run.
  echo -e "\t${bold}Not in a terminal session.${resetcolor}"
fi

# Echo the final docker run command before executing.
set -x

docker run \
  "${container_args[@]}" \
  "${configs[@]}" \
  "${build_buddy_args[@]}" \
  "${CLEANED_ARGS[@]}" \
  "${dev_image_name}" \
  "${exec_cmd[@]}"
