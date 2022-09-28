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
  echo "This script runs http2 capture for a specified amount of time on the kubernetes nodes."
  echo "It automatically finds the nodes by running kubectl, so this will run on your current-context"
  echo ""
  echo "Usage: $0 [<duration>]"
  exit
}

parse_args() {
  # Set default values.
  T=60 # Time spent running stirling on the cluster.

  # Grab arguments.
  if [ $# -gt 0 ]; then
    T=$1
  fi

  # Make sure the time argument is a number.
  # This will also cause usage to be printed if -h or any flag is passed in.
  re='^[0-9]+$'
  if ! [[ $T =~ $re ]] ; then
    usage
  fi
}

# Script execution starts here

parse_args "$@"

node_ips=$(kubectl describe nodes | grep InternalIP | cut -d ':' -f2)

date=$(date +"%Y%m%d_%H%M%S")
outdir=out/${date}/
mkdir -p ${outdir}

for node_ip in $node_ips; do
  echo $node_ip
  scp http2_capture.sh oazizi@${node_ip}:/home/oazizi > /dev/null
done

for node_ip in $node_ips; do
  echo "Running on ${node_ip}"
  ssh oazizi@${node_ip} ./http2_capture.sh $T > ${outdir}/out.${node_ip} 2> ${outdir}/err.${node_ip} &
done

wait
