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

# You'll need to specify environment variables in 'pem' container section's 'env' section
# in pem daemonset's YAML file to enable gperftool's CPU profiling. An example looks like:
#         - name: CPUPROFILE
#           value: "pem.prof"
#         - name: CPUPROFILESIGNAL
#           value: "9"
# Don't use SIGUSR1 and SIGUSR2, which were overridden by Stirling's conn_trace signal handler.

if (( $# < 1 )); then
  echo "Needs specify signal number: $0 <signal number>"
  exit 1
fi

signum=$1
echo "Using signal number ${signum} to trigger gperftools profiling ..."

# Declare an associative array to store node and pem pod.
declare -A node_pem_cid_map
declare -A node_pem_pid_map

echo "Looking for PEM processes on nodes ..."
for node in $(kubectl get nodes --no-headers | awk '{print $1}'); do
  echo "Looking for pod on node ${node} ..."
  pod=$(kubectl --namespace=pl get pods --no-headers --selector=name=vizier-pem \
    --field-selector=spec.nodeName="${node}" | awk '{print $1}')
  echo "Found pod: ${pod}"

  # --raw-output instructs jq to not output quotes.
  # Docker ID format: docker://...<ID alpha numeric ID>.
  pem_cid=$(kubectl get pod --namespace=pl "${pod}" -o json |\
    jq --raw-output '.status.containerStatuses[] | select(.name == "pem").containerID' |\
    awk -F '/' '{print $NF}')

  echo "Found PEM container: ${pem_cid}"

  pem_pid=$(gcloud compute ssh "${node}" \
    --command="sudo docker container top ${pem_cid} 2>/dev/null | tail -n 1" 2>/dev/null |\
    awk '{print $2}')
  echo "Found PEM process: ${pem_pid}"

  node_pem_cid_map["${node}"]="${pem_cid}"
  node_pem_pid_map["${node}"]="${pem_pid}"
done

# First, send signal to the processes on all nodes, to minimize time differences between processes.
for node in "${!node_pem_cid_map[@]}"; do
  pem_pid="${node_pem_pid_map[${node}]}"
  echo "Sending signal number ${signum} to ${pem_pid} on node ${node} to start CPU profiling ..."
  gcloud compute ssh "${node}" --command="sudo kill -${signum} ${pem_pid}" 2>/dev/null
done

echo "Sleep 10 minutes to wait for PEM processes to collect profiling results ..."
sleep 600

# Send signals first
for node in "${!node_pem_pid_map[@]}"; do
  pem_pid="${node_pem_pid_map[${node}]}"
  echo "Sending signal number ${signum} to ${pem_pid} on node ${node} to stop CPU profiling ..."
  gcloud compute ssh "${node}" --command="sudo kill -${signum} ${pem_pid}" 2>/dev/null
done

pem_exe="app/src/vizier/services/agent/pem/pem.runfiles/px/src/vizier/services/agent/pem/pem"

for node in "${!node_pem_pid_map[@]}"; do
  pem_cid="${node_pem_cid_map[${node}]}"
  echo
  echo "Snapshoting PEM container file system on node ${node} after finishing CPU profiling ..."
  gcloud compute ssh "${node}" --command="sudo docker export --output pem.tar ${pem_cid}" \
    2>/dev/null

  echo "Checking if pem.prof was created ..."
  # The file name format is pem.prof.<numeric ID>, we only look for the latest one,
  # which has the highest numeric ID.
  pem_prof=$(gcloud compute ssh "${node}" \
    --command="sudo tar -tvf pem.tar | grep pem.prof" \
    2>/dev/null |\
    sort --field-separator=. --key=4 --numeric-sort | tail --lines=1 | awk '{print $NF}')
  echo "Found prof file ${pem_prof} on node ${node}"

  echo "Extracting ${pem_prof} on node ${node} ..."
  gcloud compute ssh "${node}" --command="sudo tar -xvf pem.tar ${pem_prof}" 2>/dev/null

  local_pem_prof="${node}_$(basename "${pem_prof}")"
  echo "Copying file ${pem_prof} from node ${node} to ${local_pem_prof} ..."
  gcloud compute scp "${node}:~/${pem_prof}" "${local_pem_prof}" 2>/dev/null

  local_pem_exe="$(basename "${pem_exe}")"

  # Only download PEM executable once.
  if [[ ! -f "${local_pem_exe}" ]]; then
    echo "Extracting PEM executable on node ${node} ..."
    gcloud compute ssh "${node}" --command="sudo tar -xvf pem.tar ${pem_exe}" 2>/dev/null

    echo "Copying file ${pem_exe} from node ${node} to ${local_pem_exe} ..."
    gcloud compute scp "${node}:~/${pem_exe}" "${local_pem_exe}" 2>/dev/null
  fi

  echo "Use the following command to view the result:"
  echo
  echo "pprof -http :9999 ${local_pem_exe} ${local_pem_prof}"
done
