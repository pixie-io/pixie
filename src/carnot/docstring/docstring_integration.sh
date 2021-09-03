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

# --- begin runfiles.bash initialization v2 ---
# Copy-pasted from the Bazel Bash runfiles library v2.
set -uo pipefail; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v2 ---

# -- begin loading the docstring binaries ---
extractor=$(rlocation "px/src/carnot/planner/docs/doc_extractor")
if [[ ! -f "${extractor:-}" ]]; then
  echo >&2 "ERROR: could not find the doc_extractor binary"
  exit 1
fi
parser=$(rlocation "px/src/carnot/docstring/docstring_/docstring")
if [[ ! -f "${parser:-}" ]]; then
  echo >&2 "ERROR: could not find the docstring binary parser"
  exit 1
fi
# -- end loading the docstring binaries ---

# Check to make sure the output JSON is specified.
if [ $# -eq 0 ]; then
  echo "Output JSON file not specified."
  exit 1
fi

raw_docstring_pb=./input.pb.txt
echo "Extracting the raw docs to a temporary file"
${extractor} --output_file ${raw_docstring_pb}
echo "Parsing the raw docs into '$1'"
${parser} --input_doc_pb ${raw_docstring_pb} --output_json "$1"
echo "Parsing complete"
