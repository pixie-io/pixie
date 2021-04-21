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

usage() {
  echo "This scripts creates a stirling_wrapper image and runs it inside a container."
  echo ""
  echo "Usage: $0 [-i]"
  echo " -i  : interactive (enters the shell)"
  echo " -g  : push image to gcr (otherwise image is only stored locally"
  echo ""
  echo "Arguments may be passed to stirling wrapper after '--'"
  exit
}

parse_args() {
  # Set defaults here.
  INTERACTIVE=0
  USE_GCR=0

  # Process the command line arguments.
  while getopts "ig" opt; do
    case ${opt} in
      i)
        INTERACTIVE=1
        ;;
      g)
        USE_GCR=1
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

parse_args "$@"
shift $((OPTIND -1))

if [ "$USE_GCR" -eq "1" ]; then
  bazel run //src/stirling/binaries:push_stirling_wrapper_image
  image_name=gcr.io/pl-dev-infra/stirling_wrapper:${USER}
  docker pull "$image_name"
else
  bazel run //src/stirling/binaries:stirling_wrapper_image -- --norun
  image_name=bazel/src/stirling/binaries:stirling_wrapper_image
fi

flags=""
if [ "$INTERACTIVE" -eq "1" ]; then
  flags="--entrypoint sh"
fi

echo "Running image"
# shellcheck disable=SC2086
docker run -it --init --rm \
 -v /:/host \
 -v /sys:/sys \
 --env PL_HOST_PATH=/host \
 --privileged \
 --pid=host \
 --network=host \
 $flags \
 "$image_name" "$@"

# Note: Under the new syntax, mounts should be the following:
#  --mount type=bind,source=/,target=/host \
#  --mount type=bind,source=/sys,target=/sys \
# But we avoid the new syntax for compatibility with older docker versions.
