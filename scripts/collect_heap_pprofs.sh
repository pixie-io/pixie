#!/bin/bash -e

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
  echo "Usage: $0 <heap_profile_dir> <vizier_cluster_id> [--gcloud-common-args] [--gcloud-ssh-args...]"
  echo "<heap_profile_dir> : the directory where the heap profile and memory mapped files will be stored. It will be created if it does not exist."
  echo "<vizier_cluster_id> : the ID of the Vizier cluster to connect to."
  echo "[--gcloud-common-args] : common arguments to pass to gcloud commands, such as --project."
  echo "[--gcloud-ssh-args] : additional arguments to pass to gcloud compute ssh commands, such as --internal-ip."
  exit 1
}

parse_args() {
  if [ $# -lt 2 ]; then
    usage
  fi

  heap_profile_dir="$1"
  shift

  cluster_id="$1"
  shift
}

check_args() {
  if [ -z "$heap_profile_dir" ] || [ -z "$cluster_id" ]; then
    usage
  fi
}

parse_args "${@}"
check_args

script_dir=$(dirname "$(realpath "$0")")
repo_root=$(git rev-parse --show-toplevel)

mkdir -p "$heap_profile_dir"

pxl_heap_output_file="${heap_profile_dir}/heap_dump_raw_output.json"

px run -o json -c "$cluster_id" -f "${repo_root}/src/pxl_scripts/px/collect_heap_dumps.pxl"  > "$pxl_heap_output_file"

while IFS= read -r line; do
    asid=$(echo "$line" | jq -r '.asid')
    hostname=$(echo "$line" | jq -r '.hostname')
    heap_content=$(echo "$line" | jq -r '.heap')
    filename="${heap_profile_dir}/${hostname}-${asid}.txt"
    echo "$heap_content" > "$filename"
    echo "Wrote heap profile for ASID $asid on host $hostname to $filename"
done < "$pxl_heap_output_file"

declare -a agents
for file in "${heap_profile_dir}"/*.txt; do
  filename=$(basename "$file")
  filename_noext="${filename%.txt}"
  hostname="${filename_noext%-*}"
  asid="${filename_noext##*-}"

  agents["$asid"]="$hostname"
  agent_dir="${heap_profile_dir}/${hostname}-${asid}"
  mkdir -p "$agent_dir"
done

for agent in "${!agents[@]}"; do
  asid="${agent}"
  hostname="${agents[$agent]}"
  filename="${heap_profile_dir}/${hostname}-${asid}.txt"
  "${script_dir}/download_heap_prof_mapped_files.sh" "$filename" "$hostname" "${@:3}"
done
