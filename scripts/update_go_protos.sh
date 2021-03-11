#!/bin/bash -e

workspace=$(bazel info workspace 2> /dev/null)
pushd "${workspace}" &> /dev/null || exit

function label_to_path() {
  path="${1#"//"}"
  echo "${path/://}"
}

function build() {
  # Exits with message if the bazel build command goes wrong.
  # Force bazel to download all targets since bazel will delete
  # targets with minimal download mode. This is a bug in bazel:
  #   https://github.com/bazelbuild/bazel/issues/12855
  if ! out=$(bazel build --remote_download_outputs=all "$@" 2>&1); then
    echo "${out}"
    exit 1
  fi
}

function copy() {
  for label in "$@"; do
    echo "Updating ${label} ..."

    path=$(label_to_path "${label}")
    dir=$(dirname "${path}")
    name=$(basename "${path}")
    # The omitted path component tolerates the host-dependent value by bazel's go rules.
    # Also the output pb.go would be identical between host OS, so there is no need to pick any
    # particular one.
    abs_path=$(find "bazel-bin/${dir}/${name}_" -name '*.pb.go' | head -n 1)
    if [[ "${abs_path}" == "" ]]; then
      echo "Failed to locate pb.go for ${label}"
      return 1
    fi
    cp -f "${abs_path}" "${dir}"
  done
}

if [[ $# == 0 ]]; then
  mapfile -t < <(bazel query --noshow_progress --noshow_loading_progress "kind('go_proto_library rule', //...)")
else
  MAPFILE=("$@")
fi

build "${MAPFILE[@]}"
copy "${MAPFILE[@]}"
