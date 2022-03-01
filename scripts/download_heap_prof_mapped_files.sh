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
  echo "Usage: $0 <heap_profile> <node_name> [<gcloud ssh opts>...]"
  exit 1
}
set -e

heap_profile="$1"
node_name="$2"
output_dir=/tmp/prof_bins

if [ -z "$heap_profile" ] || [ -z "$node_name" ]; then
  usage
fi

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
procs=$(gcloud compute ssh --command='ps ax' "$node_name" "${@:3}" 2> "$err_file") || cat "$err_file" && rm "$err_file"

# Find the mapping that corresponds to a process on the node.
# We assume that the process was started by running one of the files in the mappings
# (which is commonly the case and certainly the case for the binaries we care about, eg. pem, kelvin).
for fname in $mappings
do
  bname=$(basename "$fname")
  matching_proc=$(echo "$procs" | grep "$bname" || true)
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
output_on_err gcloud compute ssh --command="$create_tar_cmd" "$node_name" "${@:3}"

# Copy archive to local machine.
output_on_err gcloud compute scp "${@:3}" "$USER@$node_name:~/$tar_file" "/tmp/$tar_file"

# Cleanup tar archive on node.
output_on_err gcloud compute ssh --command="rm ~/$tar_file" "$node_name" "${@:3}"

tar --strip-components=1 -C "$output_dir" -xzf "/tmp/$tar_file"

echo "Dumped mapped binaries to $output_dir"
echo "Run 'PPROF_BINARY_PATH=$output_dir pprof -http=localhost:8888 $heap_profile' to visualize the profile."
