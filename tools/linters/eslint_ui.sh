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

set -e

# This script is invoked with a line like:
# ./tools/linters/eslint_ui.sh '--format=json' --no-color --config .eslintrc.json src/ui/.../something.ts
# That assumes that there's a .eslintrc.json in the working directory. We need the nearest matching ancestor directory.

scriptDir="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
uiDir="${scriptDir}/../../src/ui"

# Walk up the lint target's directory ancestry until finding .eslintrc.json, or until reaching the UI root or /.
workDir=$(dirname "$5")
while [ ! "$workDir" = '/' ] && [ ! "$workDir" = "$uiDir" ] && [ ! -f "${workDir}/.eslintrc.json" ]; do
  workDir=$(dirname "$workDir")
done

if [ ! -f "${workDir}/.eslintrc.json" ]; then
  echo "Could not find .eslintrc when walking up the directory tree from $(dirname "$5")" >&2
  echo "Last location checked was $workDir" >&2
  exit 1
fi

pushd "${workDir}" > /dev/null
"${uiDir}/node_modules/.bin/eslint" "$@"
popd > /dev/null
