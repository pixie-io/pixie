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

set -ex

manifest_file="artifact_db_updater_job.yaml"
versions_file="src/utils/artifacts/artifact_db_updater/VERSIONS.json"

if [[ ! -f $manifest_file ]]; then
  echo "Missing manifest file"
  exit 1
fi

# Print out the versions file so we can inspect.
jq -C -r . "${versions_file}"

kubectl apply -f "${manifest_file}"
kubectl wait --for=condition=complete --timeout=60s job/artifact-db-updater-job

# Remove this after feature is enabled: https://kubernetes.io/docs/concepts/workloads/\
# controllers/jobs-run-to-completion/#clean-up-finished-jobs-automatically
kubectl delete job artifact-db-updater-job
