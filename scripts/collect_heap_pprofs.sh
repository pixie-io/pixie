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

usage() {
  echo "This script downloads all of the files listed in the mappings section of a heap profile."
  echo ""
  echo "Usage: $0 <heap_profile_dir> <vizier_cluster_id> [<gcloud ssh opts>...]"
  echo "<heap_profile_dir> : the directory where the heap profile and memory mapped files will be stored. It will be created if it does not exist."
  echo "<vizier_cluster_id> : the ID of the Vizier cluster to connect to."
  echo "Common gcloud ssh options include --project."
  exit 1
}

set -e

heap_profile_dir="$1"
cluster_id="$2"
script_dir=$(dirname "$(realpath "$0")")
repo_root=$(git rev-parse --show-toplevel)

if [ -z "$heap_profile_dir" ] || [ -z "$cluster_id" ]; then
  usage
fi

mkdir -p "$heap_profile_dir"

pxl_heap_output_file="${heap_profile_dir}/raw_output_from_hot_table_test.json"

px run -o json -c "$cluster_id" -f "${repo_root}/src/pxl_scripts/px/collect_heap_dumps.pxl"  > "$pxl_heap_output_file"

while IFS= read -r line; do
    hostname=$(echo "$line" | jq -r '.hostname')
    heap_content=$(echo "$line" | jq -r '.heap')
    echo "$heap_content" > "${heap_profile_dir}/${hostname}.txt"
    echo "Wrote ${heap_profile_dir}/${hostname}.txt"
done < "$pxl_heap_output_file"

nodes=()
for file in "${heap_profile_dir}"/*.txt; do
  hostname=$(basename "${file%.*}")
  nodes+=("$hostname")
  hostname_dir="${heap_profile_dir}/${hostname}"
  mkdir -p "$hostname_dir"
done

for node in "${nodes[@]}"; do
  "${script_dir}/download_heap_prof_mapped_files.sh" "${heap_profile_dir}/${node}.txt" "$node" "${@:3}"
done
