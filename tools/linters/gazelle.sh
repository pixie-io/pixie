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

tmpdir=$(mktemp -d)
patchFile="${tmpdir}/diff.patch"
builddir=$(dirname "$1")

if bazel run //:gazelle --noshow_progress -- --mode=diff "$builddir" 2>/dev/null 1>"${patchFile}"; then
  exit 0
else
  # Here be dragons, beware all who step here.
  # gazelle responds with a diff in patch format, however afaik
  # for some reason arcanist still doesn't accept patch formats.
  # arcanist wants patches as the set of 4 pieces of info:
  # {line, char, original_text, replacement_text}
  # So we just patch the entire file, and hand arcanist the original, and new contents
  # with line, char set to 1, 1.

  fileToPatch=$(grep '^---' "$patchFile" | perl -pe 's/--- (.*)\t.*/\1/g')
  correctedFileContents=$(patch "$fileToPatch" "$patchFile" -o- 2>/dev/null)
  originalFileContents=$(cat "$fileToPatch")

  # Careful, do not modify this output without also modifyng the matcher in .arclint
  # The arcanist linter regex is sensitive to exact text and newlines.
  echo "error" # severity
  echo "$fileToPatch" # file
  echo "Gazelle was not run on file" # message
  echo "1,1"  # line,char
  echo "<<<<<"
  echo "$originalFileContents" # original
  echo "====="
  echo "$correctedFileContents" # replacement
  echo ">>>>>"
fi
