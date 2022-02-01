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

bazel build --bes_backend="" --bes_results_url="" @com_github_bazelbuild_buildtools//buildifier:buildifier 2>/dev/null 1>/dev/null
bazel-bin/external/com_github_bazelbuild_buildtools/buildifier/buildifier_/buildifier \
--mode=check \
--lint=warn \
--warnings=+out-of-order-load,+unsorted-dict-items,+native-cc,+native-java,+native-proto,+native-py,-module-docstring,-function-docstring,-function-docstring-args,-function-docstring-header \
"$1" 2>&1 | sed "s| # reformat|:1: reformat: File needs to be formatted, please run \`make buildifier\`|"
