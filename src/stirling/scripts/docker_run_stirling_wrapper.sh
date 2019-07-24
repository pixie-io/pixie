#!/bin/bash

set -e

usage() {
  echo "Usage: $0 [-i]"
  echo " -i          : interactive (enters the shell)"
  exit
}

parse_args() {
  INTERACTIVE=0

  local OPTIND
  # Process the command line arguments.
  while getopts "i" opt; do
    case ${opt} in
      i)
        INTERACTIVE=1
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

bazel run //src/stirling:stirling_wrapper_image

flags=""
if [ "$INTERACTIVE" -eq "1" ]; then
  flags="--entrypoint sh"
fi

docker run -it --init --rm \
 --mount type=bind,source=/proc,target=/host/proc \
 --mount type=bind,source=/sys,target=/sys \
 --mount type=bind,source=/usr/src,target=/host/usr/src \
 --mount type=bind,source=/lib/modules,target=/host/lib/modules \
 --env PL_PROC_PATH=/host/proc \
 --privileged \
 $flags \
 bazel/src/stirling:stirling_wrapper_image
