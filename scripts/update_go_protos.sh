#!/bin/bash -e

if (( $# > 1 )); then
  echo "Needs at most one argument as a build label, exit ..."
  exit 1
fi

git_root=$(git rev-parse --show-toplevel)
if [[ "${git_root}" != "$(pwd)" ]]; then
  echo "Must run this at the top of tree of the git repo, exit ..."
  exit 1
fi

function label_to_path() {
  path="${1#"//"}"
  echo "${path/://}"
}

function update_one_build_label() {
  build_label="$1"
  echo "Updating ${build_label} ..."

  # Exits with message if the bazel build command goes wrong.
  if ! out=$(bazel build "${build_label}" 2>&1); then
    echo "${out}"
    exit 1
  fi

  path=$(label_to_path "${build_label}")
  dir=$(dirname "${path}")
  name=$(basename "${path}")
  # The omitted path component tolerates the host-dependent value by bazel's go rules.
  # Also the output pb.go would be identical between host OS, so there is no need to pick any
  # particular one.
  abs_path=$(find "bazel-bin/${dir}/${name}_" -name '*.pb.go' | head -n 1)
  if [[ "${abs_path}" == "" ]]; then
    echo "Failed to located pb.go file at ${abs_path}"
    return 1
  fi
  cp -f "${abs_path}" "${dir}"
}

function update_all_build_labels() {
  for label in $(bazel query "kind('go_proto_library rule', //...)"); do
    update_one_build_label "${label}"
  done
}

if [[ $# == 0 ]]; then
  update_all_build_labels
fi

if [[ $# == 1 ]]; then
  build_label="$1"
  if [[ ! "${build_label}" == "//"* ]]; then
    echo "Build label must be fully qualified with '//' at the beginning, exit ..."
    exit 1
  fi
  if [[ "${build_label}" != *"pl_go_proto" ]]; then
    echo "Expect the build label ends with 'pl_go_proto', exit ..."
    exit 1
  fi
  update_one_build_label "$1"
fi
