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

if [[ -z "${PR_BODY}" ]]; then
  echo "No PR body found"
  exit 2
fi

# Grep doesn't handle carriage returns very well, so remove them.
PR_BODY="$(echo "${PR_BODY}" | sed 's/\r//g')"

# Remove any detail blocks from the PR_BODY before linting.
PR_BODY="$(echo "${PR_BODY}" | sed '/<details>/,/<\/details>/d')"

# Remove any code blocks from the PR_BODY before linting.
# shellcheck disable=SC2016
PR_BODY="$(echo "${PR_BODY}" | sed '/```/,/```/{/```/!d}')"

failed="false"
bad_description() {
  msg="$1"
  echo ""
  echo -e "$msg"
  failed="true"
}

# Check that there's a summary.
echo "$PR_BODY" | grep -E "^Summary: .+" > /dev/null || bad_description "PR description must include Summary: <summary>"

# Check that there's a test plan.
echo "$PR_BODY" | grep -E "^Test Plan: .+" > /dev/null || bad_description "PR description must include Test Plan: <test plan>"

# Check that there's a Type of Change field.
echo "$PR_BODY" | grep -E "^Type of change: /kind \w+" > /dev/null || bad_description "PR description must include Type of change: /kind <change_kind>"

# Check that no lines begin with whitespace.
lines_start_whitespace="$(echo "$PR_BODY" | grep -E '^.+$' | grep -nE "^\s" || true)"
[[ -z "${lines_start_whitespace}" ]] || bad_description "Line(s) begin with whitespace:\n${lines_start_whitespace}"

# Check that no lines end with whitespace.
lines_end_whitespace="$(echo "$PR_BODY" | grep -nE "\s$" || true)"
[[ -z "${lines_end_whitespace}" ]] || bad_description "Line(s) end with whitespace:\n${lines_end_whitespace}"

# Check that each "Key:" has text after it (except for Changelog Message which is allowed to have a codeblock start on the next line)
keys_without_text="$(echo "$PR_BODY" | grep -E "^[a-zA-Z ]+:" | grep -v "Changelog Message:" | grep -nE "^[a-zA-Z ]+:\s*$" || true)"
[[ -z "${keys_without_text}" ]] || bad_description "Each 'Key:' clause must have text after it:\n${keys_without_text}"

# Check that there's a newline before each "Key:" clause (except if the clause is at the start of the message).
keys_without_newlines="$(echo "$PR_BODY" | grep -E -B 1 "^[a-zA-Z ]+:" | grep -Ev "^[a-zA-Z ]+:" | grep -v "\-\-" | sed '/^$/d' )"
[[ -z "${keys_without_newlines}" ]] || bad_description "Each 'Key:' clause must have a newline before it, add newlines after:\n${keys_without_newlines}"


if [[ "${failed}" == "true" ]]; then
  exit 100
fi
