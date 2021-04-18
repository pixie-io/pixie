#!/usr/bin/env bash

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

shopt -s globstar

function replace_pb_paths() {
  # Replaces the import paths for protobufs into the new directory structure.
  PROTO_NAMESPACE="pxapi"
  for file in "$@"; do
    sed -i \
      -e "s/^from src.api.public.cloudapipb/from ${PROTO_NAMESPACE}.cloudapipb/g" \
      -e "s/^from src.api.public.vizierapipb/from ${PROTO_NAMESPACE}.vizierapipb/g" \
      -e "s/^from src.api.public.uuidpb/from ${PROTO_NAMESPACE}.uuidpb/g" \
      "${file}"
  done
}

function prepare_python_repo() {
  if [ $# -lt 3 ]; then
    echo "No destination dir provided"
    exit 1
  fi

  PYTHONPKG="${1%/}"
  TOOLPATH="${2%/}"
  TMPDIR="${3%/}"
  mkdir -p "$TMPDIR"

  echo "$(date) : === Preparing python package files in: ${TMPDIR}"

  BAZELPATH=bazel-bin/"${TOOLPATH}.runfiles"

  if [ ! -d "${BAZELPATH}" ]; then
    echo "Could not find bazel path for tool: '${BAZELPATH}' Did you run from the root of the build tree?"
    exit 1
  fi

  # PIXIEPKG="${TMPDIR}/pxapi"
  cp -LR \
    "${BAZELPATH}/pl/${PYTHONPKG}/pxapi" \
    "${TMPDIR}/pxapi"

  # Add README and setup.py
  cp "${PYTHONPKG}/README.md" "${TMPDIR}"
  cp "${PYTHONPKG}/setup.py" "${TMPDIR}"
  cp "${PYTHONPKG}/requirements.txt" "${TMPDIR}"

  echo "$(date) : === Done preparing python package files in: ${TMPDIR}"
}
