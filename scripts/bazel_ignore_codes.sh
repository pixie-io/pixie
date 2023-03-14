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

set +e
bazel "$@"
retval="$?"
# 4 means that tests not present.
# 38 means that bes update failed.
# Both are not fatal.
if [[ "${retval}" == 4 ]] || [[ "${retval}" == 38 ]]; then
  echo "Bazel returned code ${retval}, ignoring..." >&2
  exit 0
fi
exit "${retval}"
