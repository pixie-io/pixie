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

# Clean-up any spawned background processes on exit.
trap 'kill $(jobs -p) &> /dev/null || true' SIGINT SIGTERM EXIT

# shellcheck source=./src/stirling/scripts/utils.sh
source src/stirling/scripts/utils.sh

if [ -z "$BUILD_WORKSPACE_DIRECTORY" ] && [ -z "$TEST_TMPDIR" ]; then
    echo "Building stirling_wrapper_image ..."
    stirling_image=$(bazel_build //src/stirling/binaries:stirling_wrapper_image.tar)
    echo "Building java_image ..."
    java_image=$(bazel_build //src/stirling/source_connectors/jvm_stats/testing:hello_world_image.tar)
    echo "Building java_app ..."
    java_app=$(bazel_build //src/stirling/source_connectors/jvm_stats/testing:HelloWorld)
    java_app=${java_app%.jar}
else
    # If the script was run through bazel, the locations are passed as arguments.
    stirling_image=$1
    java_image=$2
    java_app=$3
fi

###############################################################################
# Test set-up
###############################################################################

echo "Loading $java_image ..."
java_image_name=$(docker_load "$java_image")
echo "java_image_name: $java_image_name"

echo "Loading $stirling_image ..."
stirling_wrapper_image_name=$(docker_load "$stirling_image")
echo "stirling_wrapper_image_name: $stirling_wrapper_image_name"

java_container_name="java_$$_$RANDOM"
echo "Launching $java_image_name in container $java_container_name ..."
docker run --rm --detach --name "$java_container_name" "$java_image_name"

java_tmp_mount_container_name="java_tmp_mount_$$_$RANDOM"
echo "Launching $java_image_name in container $java_tmp_mount_container_name ..."
docker run --rm --detach --name "$java_tmp_mount_container_name" --volume=/tmp:/tmp "$java_image_name"

echo "Launching $java_app directly ..."
$java_app &>/dev/null &
java_app_pid=$!
echo "java_app_pid: $java_app_pid"

sleep 2

stirling_wrapper_container_name="stirling_wrapper_$$_$RANDOM"
echo "Launching $stirling_wrapper_image_name in container $stirling_wrapper_container_name ..."
docker run --init --rm --detach --privileged --detach --name $stirling_wrapper_container_name \
  --volume /:/host --volume /sys:/sys \
  --env PL_HOST_PATH=/host \
 "$stirling_wrapper_image_name" --print_record_batches=jvm_stats

# Wait for stirling to initialize.
for _ in {1..60}; do
  sleep 1
  stirling_wrapper_output=$(docker container logs $stirling_wrapper_container_name 2>&1)
  if echo "$stirling_wrapper_output" | grep -q "Probes successfully deployed."; then
    break
  fi
done

sleep 10

java_pid=$(docker container inspect -f '{{.State.Pid}}' $java_container_name)
java_tmp_mount_pid=$(docker container inspect -f '{{.State.Pid}}' $java_tmp_mount_container_name)

echo "stirling_wrapper output: $stirling_wrapper_output"
echo "Java PID: $java_pid"

docker_stop $stirling_wrapper_container_name
docker_stop $java_container_name
docker_stop $java_tmp_mount_container_name
kill $java_app_pid

function check_pid() {
  local output="$1"
  local pid="$2"
  success_msg=$(echo "$output" | grep -cE -e "\[JVMStats\].+$pid" || true)
  if [[ "$success_msg" == "0" ]]; then
    echo "Could not find Java PID $pid in stirling_wrapper output"
    echo "Test FAILED"
    return 1
  else
    echo "Found Java PID $pid in stirling_wrapper output"
    echo "Test PASSED"
    return 0
  fi
}

check_pid "$stirling_wrapper_output" "$java_pid"
check_pid "$stirling_wrapper_output" "$java_tmp_mount_pid"
check_pid "$stirling_wrapper_output" "$java_app_pid"
