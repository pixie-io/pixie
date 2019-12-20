#!/bin/bash

set -e

usage() {
  echo "This scripts creates a stirling_wrapper image and runs it inside a container."
  echo ""
  echo "Usage: $0 [-i]"
  echo " -i  : interactive (enters the shell)"
  echo " -g  : push image to gcr (otherwise image is only stored locally"
  exit
}

parse_args() {
  # Set defaults here.
  INTERACTIVE=0
  USE_GCR=0

  local OPTIND
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

if [ "$USE_GCR" -eq "1" ]; then
  bazel run //src/stirling:push_stirling_wrapper_image
  image_name=gcr.io/pl-dev-infra/stirling_wrapper:${USER}
  docker pull "$image_name"
else
  bazel run //src/stirling:stirling_wrapper_image -- --norun
  image_name=bazel/src/stirling:stirling_wrapper_image
fi

flags=""
if [ "$INTERACTIVE" -eq "1" ]; then
  flags="--entrypoint sh"
fi

echo "Running image"
# shellcheck disable=SC2086
docker run -it --init --rm \
 --mount type=bind,source=/,target=/host \
 --mount type=bind,source=/sys,target=/sys \
 --env PL_PROC_PATH=/host/proc \
 --privileged \
 $flags \
 "$image_name"
