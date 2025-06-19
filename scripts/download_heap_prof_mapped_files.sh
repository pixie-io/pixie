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
  echo "Usage: $0 <heap_profile> <node_name> [--gcloud-common-args] [--gcloud-ssh-args...]"
  echo "<heap_profile> : the path to the heap profile file (in pprof format) to analyze."
  echo "<node_name> : the name of the node to connect to (e.g., the name of a Vizier agent node)."
  echo "[--gcloud-common-args] : common arguments to pass to gcloud commands, such as --project."
  echo "[--gcloud-ssh-args] : additional arguments to pass to gcloud compute ssh commands, such as --internal-ip."
  exit 1
}

parse_args() {
  if [ $# -lt 2 ]; then
    usage
  fi

  heap_profile="$1"
  shift

  node_name="$1"
  shift

  while test $# -gt 0; do
    case "$1" in
      --gcloud-common-args=*)
        GCLOUD_COMMON_ARGS="${1#*=}"
        shift
        ;;
      --gcloud-common-args)
        GCLOUD_COMMON_ARGS="$2"
        shift
        shift
        ;;
      --gcloud-ssh-args=*)
        GCLOUD_SSH_ARGS="${1#*=}"
        shift
        ;;
      --gcloud-ssh-args)
        GCLOUD_SSH_ARGS="$2"
        shift
        shift
        ;;
      *) usage ;;
    esac
  done;
}


check_args() {
  if [ -z "$heap_profile" ] || [ -z "$node_name" ]; then
    usage
  fi
}

parse_args "${@}"
check_args

output_dir="${heap_profile%.txt}"

# Create the output directory at the beginning of the script.
mkdir -p "$output_dir"

# Get the list of mappings in the heap profile. Pprof formatted profiles have a section at the bottom that looks like:
#  MAPPED_LIBRARIES:
#  00200000-01626000 r--p 00000000 00:00 6709979     /app/src/vizier/services/agent/pem/pem.runfiles/px/src/vizier/services/agent/pem/pem
#  01626000-0579e000 r-xp 01425000 00:00 6709979     /app/src/vizier/services/agent/pem/pem.runfiles/px/src/vizier/services/agent/pem/pem
#  ...
# This awk command grabs the list of all file names from this section of the heap profile.
mappings=$(awk 'BEGIN{m=0} /MAPPED_LIBRARIES/{m=1} { if(m) { print $6 }}' "$heap_profile" | grep "^/" | sort | uniq)

err_file="$output_dir/gcloud_error.log"
zone=$(gcloud compute instances list "${GCLOUD_COMMON_ARGS}" --filter="$node_name" --format="table(name, zone)"| tail -n 1 | awk '{print $2}')
procs=$(gcloud compute ssh --zone "$zone" --command='ps ax' "$node_name" "${GCLOUD_COMMON_ARGS}" "${GCLOUD_SSH_ARGS}" 2> "$err_file") || cat "$err_file" && rm "$err_file"

# Find the mapping that corresponds to a process on the node.
# We assume that the process was started by running one of the files in the mappings
# (which is commonly the case and certainly the case for the binaries we care about, eg. pem, kelvin).
for fname in $mappings
do
  bname=$(basename "$fname")
  matching_proc=$(echo "$procs" | grep -E "$bname$" || true)
  if [[ -n "$matching_proc" ]]; then
    pid=$(echo "$matching_proc" | awk '{ print $1 }')
    matched_file="$fname"
  fi
done

echo "Using PID ($pid) of $matched_file"

file_root="/proc/$pid/root"
file_paths=$(echo "$mappings" | xargs -I{} echo "$file_root{}")

# Create a tar archive on the remote node with each of the files from the mappings found above.
tar_file="output.tar.gz"
create_tar_cmd=$(cat << EOF
 mkdir -p output;
 echo "$file_paths" | xargs -I{} sudo cp {} output;
 tar -czf "$tar_file" output/*;
 rm -rf output;
EOF
)

output_on_err() {
  output=$("$@" 2>&1) || echo "$output"
}

# Create tar archive on node.
output_on_err gcloud compute ssh  --zone "$zone" --command="$create_tar_cmd" "$node_name" "${GCLOUD_COMMON_ARGS}" "${GCLOUD_SSH_ARGS}"

# Copy archive to local machine.
output_on_err gcloud compute scp --zone "$zone" "${GCLOUD_COMMON_ARGS}" "${GCLOUD_SSH_ARGS}" "$USER@$node_name:~/$tar_file" "${output_dir}/$tar_file"

# Cleanup tar archive on node.
output_on_err gcloud compute ssh --zone "$zone" --command="rm ~/$tar_file" "$node_name" "${GCLOUD_COMMON_ARGS}" "${GCLOUD_SSH_ARGS}"

tar --strip-components=1 -C "$output_dir" -xzf "${output_dir}/$tar_file"

echo "Dumped mapped binaries to $output_dir"
echo "Run 'PPROF_BINARY_PATH=$output_dir pprof -http=localhost:8888 $heap_profile' to visualize the profile."
