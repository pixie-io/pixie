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

BUNDLE_FILE="$1"
YQ_BIN="$2"

if [ ! -f "$BUNDLE_FILE" ]; then
    echo "Error: Bundle file not found: $BUNDLE_FILE"
    exit 1
fi

# Check that scripts object has keys
NUM_SCRIPTS=$("$YQ_BIN" eval '.scripts | keys | length' "$BUNDLE_FILE")

if [ "$NUM_SCRIPTS" -eq 0 ]; then
    echo "Error: No scripts found in bundle"
    exit 1
fi

echo "Success: Found $NUM_SCRIPTS scripts in bundle"
