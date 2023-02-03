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

if [[ -z "${COMMIT_MESSAGE}" ]]; then
  echo "No commit message"
  exit 2
fi

# Check that there's a summary.
echo "$COMMIT_MESSAGE" | grep -E "^Summary: .+" > /dev/null || (echo "Commit message must include Summary: <summary>" && exit 100)

# Check that there's a test plan.
echo "$COMMIT_MESSAGE" | grep -E "^Test Plan: .+" > /dev/null || (echo "Commit message must include Test Plan: <test plan>" && exit 100)

# Check that there's a Type of Change field.
echo "$COMMIT_MESSAGE" | grep -E "^Type of change: /kind \w+" > /dev/null || (
  echo "Commit message must include Type of change: /kind <change_kind>" && exit 100
)

# Check that no lines begin with whitespace.
lines_start_whitespace="$(echo "$COMMIT_MESSAGE" | grep -nE "^\W" || true)"
[[ -z "${lines_start_whitespace}" ]] || (echo -e "Line(s) begin with whitespace:\n${lines_start_whitespace}" && exit 100)

# Check that no lines end with whitespace.
lines_end_whitespace="$(echo "$COMMIT_MESSAGE" | grep -nE "\s$" || true)"
[[ -z "${lines_end_whitespace}" ]] || (echo -e "Line(s) end with whitespace:\n${lines_end_whitespace}" && exit 100)

# Check that each "Key:" has text after it.
keys_without_text="$(echo "$COMMIT_MESSAGE" | grep -E "^[a-zA-Z ]+:" | grep -nE "^[a-zA-Z ]+:\s*$" || true)"
[[ -z "${keys_without_text}" ]] || (echo -e "Each 'Key:' clause must have text after it:\n${keys_without_text}" && exit 100)

# Check that there's a newline before each "Key:" clause (except if the clause is at the start of the message).
keys_without_newlines="$(echo "$COMMIT_MESSAGE" | grep -E -B 1 "^[a-zA-Z ]+:" | grep -Ev "^[a-zA-Z ]+:" | grep -v "\-\-" | sed '/^$/d' )"
[[ -z "${keys_without_newlines}" ]] || (
  echo -e "Each 'Key:' clause must have a newline before it, add newlines after:\n${keys_without_newlines}" && exit 100
)
