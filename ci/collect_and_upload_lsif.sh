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

SOURCEGRAPH_ENDPOINT="https://cs.corp.pixielabs.ai"

GIT_COMMIT=""
SOURCEGRAPH_ACCESS_TOKEN=""

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-t <sourcegraph_token>] [-c <git_commit>]" 1>&2;
  exit 1;
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "c:t:h" opt; do
    case ${opt} in
      c)
        GIT_COMMIT=$OPTARG
        ;;
      t)
        SOURCEGRAPH_ACCESS_TOKEN=$OPTARG
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

# Parse the input arguments.
parse_args "$@"

export SRC_ENDPOINT="${SOURCEGRAPH_ENDPOINT}"
export SRC_ACCESS_TOKEN="${SOURCEGRAPH_ACCESS_TOKEN}"

upload_to_sourcegraph() {
  /opt/pixielabs/bin/src lsif upload \
  -repo=github.com/pixie-labs/pixielabs \
  -file="$1" \
  -commit="${GIT_COMMIT}" \
  -ignore-upload-failure \
  -no-progress
}

pushd "$(bazel info workspace)"

# lsif-go has a loadPackage stage which seems to fail when it encounters a cgo import.
# Explicitly calling go mod download fixes this.
go mod download

LSIF_GO_OUT="go.dump.lsif"
/opt/pixielabs/bin/lsif-go \
  --verbose \
  --no-animation \
  --output="${LSIF_GO_OUT}"
upload_to_sourcegraph "${LSIF_GO_OUT}"

# Disabling TS LSIF upload until sourcegraph fix their fork of lsif-tsc.
# LSIF_TS_OUT="ts.dump.lsif"
# lsif-tsc --out "${LSIF_TS_OUT}" -p src/ui
# upload_to_sourcegraph "${LSIF_TS_OUT}"

LSIF_CPP_OUT="cpp.dump.lsif"
mapfile -t < <(bazel query \
  --noshow_progress \
  --noshow_loading_progress \
  'kind("cc_(library|binary|test|proto_library) rule",//... -//third_party/... -//experimental/...) except attr("tags", "manual", //...)')
./scripts/gen_compilation_database.py \
  --run_bazel_build \
  --include_genfiles \
  "${MAPFILE[@]}"
lsif-clang \
  --extra-arg="-resource-dir=$(clang -print-resource-dir)" \
  --out="${LSIF_CPP_OUT}" \
  compile_commands.json
upload_to_sourcegraph "${LSIF_CPP_OUT}"

popd
