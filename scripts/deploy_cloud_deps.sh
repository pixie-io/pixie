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

if [ "$#" -ne 1 ]; then
  echo "This script requires exactly one argument: <deploy_env : dev, prod, etc.>"
  exit 1
fi
deploy_env=$1

workspace=$(git rev-parse --show-toplevel)

# shellcheck source=scripts/script_utils.sh
source "${workspace}"/scripts/script_utils.sh

cloud_deps_deploy() {
  # Can't use kubectl -k because bundled kustomize is v2.0.3 as of kubectl v1.16, vs kustomize which is v3.5.4
  kustomize build "${workspace}"/k8s/cloud_deps/base/elastic/operator | kubectl apply -f -
  kustomize build "${workspace}"/k8s/cloud_deps/"${deploy_env}" | kubectl apply -f -
}

retry cloud_deps_deploy 5 30
