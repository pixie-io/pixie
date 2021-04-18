#!/bin/bash

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

# --- begin runfiles.bash initialization ---
# Load Bazel's Bash runfiles library (tools/bash/runfiles/runfiles.bash).
set -euo pipefail
if [[ ! -d "${RUNFILES_DIR:-/dev/null}" && ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  if [[ -f "$0.runfiles_manifest" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles_manifest"
  elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
  elif [[ -f "$0.runfiles/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    export RUNFILES_DIR="$0.runfiles"
  fi
fi
if [[ -f "${RUNFILES_DIR:-/dev/null}/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
  # shellcheck disable=1090
  source "${RUNFILES_DIR}/bazel_tools/tools/bash/runfiles/runfiles.bash"
elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  # shellcheck disable=1090
  source "$(grep -m1 "^bazel_tools/tools/bash/runfiles/runfiles.bash " \
    "$RUNFILES_MANIFEST_FILE" | cut -d ' ' -f 2-)"
else
  echo >&2 "ERROR: cannot find @bazel_tools//tools/bash/runfiles:runfiles.bash"
  exit 1
fi
# --- end runfiles.bash initialization ---

set -e
shopt -s globstar
source src/api/python/copy_python_srcs.sh

function real_path() {
  is_absolute "$1" && echo "$1" || echo "$PWD/${1#./}"
}
function is_absolute() {
  [[ "$1" = /* ]] || [[ "$1" =~ ^[a-zA-Z]:[/\\].* ]]
}

wait_for_user() {
  local c
  echo
  read -r -p "Continue (Y/n): " c
  # We test for \r and \n because some stuff does \r instead.
  if ! [[ "$c" == '' || "$c" == $'\r' || "$c" == $'\n' || "$c" == 'Y' || "$c" == 'y' ]]; then
    exit 1
  fi
  echo
}

function prepare_go_src() {
  if [ $# -lt 1 ]; then
    echo "No destination dir provided"
    exit 1
  fi

  TMPDIR="${1%/}"
  mkdir -p "$TMPDIR"

  TOOLPATH="src/api"

  cp -LR \
    "bazel-bin/${TOOLPATH}/copy_to_public_repo.runfiles/pl/src/api/go" \
    "${TMPDIR}"
}

function copy_go_runfiles() {
  if [ $# -lt 2 ]; then
    echo "No destination dir provided"
    exit 1
  fi

  CURDIR="${1%/}"
  TMPDIR="${2%/}"
  mkdir -p "$TMPDIR"

  TOOLPATH="src/api"

  cp -LR \
    "bazel-bin/${TOOLPATH}/copy_to_public_repo.runfiles/pl/${CURDIR}" \
    "${TMPDIR}"

  # ls "${TMPDIR}"/**/*.go
}

function replace_go_paths() {
  # Replaces the import paths for protobufs into the new directory structure.
  for file in "$@"; do
    sed -i \
      -e "s:px.dev/pixie:go.withpixie.dev/pixie:g" \
      "${file}"
  done
}

function usage() {
  echo "Usage:"
  echo "$0 [--pixie pixiedir] [--force]"
  echo "$0 pixiedir [--force]"
  echo ""
  echo "    pixiedir              Directory to use for pixie"
  echo "                              will fail if not specified."
  echo ""
  echo "    --force               Delete existing \`pixiedir\`/src/api dirs if they exist"
  echo "                              Will ask for permission otherwise."
  echo ""
  exit 1
}

function main() {
  set +u
  PIXIE_DIR=""
  FORCE=false

  if [ $# -eq 0 ]; then
    usage
    exit 1
  fi

  # Process args.
  while true; do
    if [[ "$1" == "--help" ]]; then
      usage
      exit 1
    elif [[ "$1" == "--pixie" ]]; then
      shift
      PIXIE_DIR=$(real_path "$1")
    elif [[ "$1" == "--force" ]]; then
      FORCE=true
    else
      PIXIE_DIR=$(real_path "$1")
    fi
    shift

    if [[ -z "$1" ]]; then
      break
    fi
  done

  DEST_DIR="${PIXIE_DIR}"
  if [ ! -d "${DEST_DIR}" ]; then
    echo "${DEST_DIR} does not exist. You must specify an existing dir"
    exit 1
  fi
  echo "$(date) : === Copying files to: ${DEST_DIR}"

  API_DIR="${DEST_DIR}"/src/api
  if [ -d "${API_DIR}" ]; then
    if [ "$FORCE" = false ]; then
      echo "Are you ok to delete existing '${API_DIR}'? (It should be saved in your git history)."
      read -p "(y/n) " -n 1 -r
      if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
      fi
    fi
    rm -rf "${API_DIR}"
  fi

  PY_DIR="${API_DIR}/python"
  GO_DIR="${API_DIR}/go"

  if [ -d "${PY_DIR}" ]; then
    echo "${PY_DIR} already exists."
    exit 1
  fi

  if [ -d "${GO_DIR}" ]; then
    echo "${GO_DIR} already exists."
    exit 1
  fi

  prepare_python_repo "src/api/python" "src/api/copy_to_public_repo" "${PY_DIR}"

  copy_go_runfiles "src/api/go" "${API_DIR}"
  copy_go_runfiles "src/api/go_examples" "${API_DIR}"
  copy_go_runfiles "src/api/public" "${API_DIR}"
  copy_go_runfiles "src/api/python_examples" "${API_DIR}"

  # -- begin loading prepend_licenses ---
  prepend_licenses=$(rlocation "px/src/api/prepend_licenses")
  if [[ ! -f "${prepend_licenses:-}" ]]; then
    echo >&2 "ERROR: could not find the prepend_licenses binary"
    exit 1
  fi
  # -- end loading prepend_licenses ---

  # Preprint licenses.
  ${prepend_licenses} "${API_DIR}"

  # Replace the go paths with the new package form.
  replace_go_paths "${DEST_DIR}"/**/*.go
}

main "$@"
