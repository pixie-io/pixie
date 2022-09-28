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
  echo "This script runs http2 capture for a specified amount of time."
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

# Ports to monitor as HTTP2/GRPC
ports=(3550 5000 5050 7000 7070 8080 9555 8080)

options=""
for port in "${ports[@]}"; do
  options="${options} -d tcp.port==${port},http2 "
done

tshark -i any \
 -Y "http2.data.data" \
 ${options} \
 -Tjson \
 -e ip.src \
 -e ip.dst \
 -e tcp.srcport \
 -e tcp.dstport \
 -e http2.streamid \
 -e http2.type \
 -e http2.headers \
 -e http2.headers.method \
 -e http2.headers.path \
 -e http2.headers.scheme \
 -e http2.headers.authority \
 -e http2.headers.status \
 -e http2.data.data \
 -e grpc.message_data \
 -e protobuf \
 &

pid=$!

sleep $T

kill $pid
