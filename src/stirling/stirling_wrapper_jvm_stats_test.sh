#!/bin/bash -e

# Clean-up any spawned background processes on exit.
trap 'kill $(jobs -p) &> /dev/null || true' SIGINT SIGTERM EXIT

script_dir="$(dirname "$0")"

# shellcheck source=./src/stirling/scripts/utils.sh
source "$script_dir"/scripts/utils.sh

if [ -z "$BUILD_WORKSPACE_DIRECTORY" ] && [ -z "$TEST_TMPDIR" ]; then
    pixie_root="$script_dir"/../..
    echo "Building stirling_wrapper_image ..."
    stirling_image=$pixie_root/$(bazel_build //src/stirling:stirling_wrapper_image.tar)
    echo "Building java_image ..."
    java_image=$pixie_root/$(bazel_build //src/stirling/testing/java:hello_world_image.tar)
    echo "Building java_app ..."
    java_app=$pixie_root/$(bazel_build //src/stirling/testing/java:HelloWorld)
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
