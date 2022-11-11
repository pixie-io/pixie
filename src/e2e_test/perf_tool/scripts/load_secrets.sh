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

workspace=$(git rev-parse --show-toplevel)
namespace="px-perf"
credentials_path=${workspace}/private/credentials/dev_infra/perf_tool

# Apply secrets.
for yaml in "${credentials_path}"/*.yaml; do
  echo "Loading: ${yaml}"
  sops --decrypt "${yaml}" | kubectl apply -n "${namespace}" -f -
done
