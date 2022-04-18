#!/bin/bash -ex

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


function usage() {
  # Disable the printing on each echo.
  set +x
  echo "Usage:"
  echo "$0 namespace --help"
  echo ""
  echo "    namespace             The namespace where we want to load the db"
  echo "                              Required."
  echo ""
  exit 1
}

if [ $# -lt 1 ]; then
  usage
  exit
fi

namespace=""
repo_path=$(bazel info workspace)
versions_file="${repo_path}/src/utils/artifacts/artifact_db_updater/VERSIONS.json"
while true; do
    if [[ "$1" == "--help" ]]; then
        usage
        exit 1
    else
        namespace=$1
    fi
    shift

    if [[ -z "$1" ]]; then
        break
    fi
done

# Port-forward to a different port than postgres to avoid problems.
# If you are having troubles in the future, check overlapping `sudo lsof -i :<postgres_port>`
# (substitute manually) and edit the script to a different port if it's not empty
postgres_port=35432
# Port-forward the postgres pod.
postgres_pod=$(kubectl get pod --namespace "$namespace" --selector="name=postgres" \
    --output jsonpath='{.items[0].metadata.name}')
kubectl port-forward pods/"$postgres_pod" ${postgres_port}:5432 -n "$namespace" &

# Update database with Vizier versions.
bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name vizier --versions_file "${versions_file}"
bazel run -c opt //src/utils/artifacts/artifact_db_updater:artifact_db_updater -- \
    --versions_file "${versions_file}" --postgres_db "pl" --postgres_port "${postgres_port}"

# Update database with CLI versions.
bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"
bazel run -c opt //src/utils/artifacts/artifact_db_updater:artifact_db_updater -- \
    --versions_file "${versions_file}" --postgres_db "pl" --postgres_port "${postgres_port}"

# Update database with operator versions.
bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name operator --versions_file "${versions_file}"
bazel run -c opt //src/utils/artifacts/artifact_db_updater:artifact_db_updater -- \
    --versions_file "${versions_file}" --postgres_db "pl" --postgres_port "${postgres_port}"

git checkout main "$versions_file"

# Kill kubectl port-forward.
kill -15 "$!"
sleep 2

# Double check that it's dead.
if pidof "$!"; then
  kill -9 "$!"
fi
