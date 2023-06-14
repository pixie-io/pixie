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

set -e

git rev-parse HEAD > GIT_COMMIT

if [[ "${TAG_NAME}" == *"/v"* ]]; then
  # Valid tag is present so we write it to the versions file.
  echo "${TAG_NAME##*/v}" > VERSION
else
  # No valid tag, treat it as a dev build.
  # We automatically set the last digit to the build number.

  # Semver tags can't contain extra "-"'s, so we need to remove them from the
  # job name.
  sanitized_job_name=$(echo "${JOB_NAME}" | sed -r 's/[\/%]//g')
  echo "0.0.${BUILD_NUMBER}-${sanitized_job_name}-dev" > VERSION
fi
