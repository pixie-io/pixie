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

# This script goes through all versions of nodejs from docker hub, between 12.3.1 and 14.18.1.
# For each node version, use its etag to replace the existing node_15_0 build rule, and test the
# openssl_trace_bpf_test, which will pick up the new docker image and use it to run server.
#
# One can specify a range of semantic versions:
#   nodejs_openssl_test.sh <lower version> <higher version>

# Return true if $1 is less than $2 as 2 semantic versions.
function verlte() {
    [[  "$1" == "$(echo -e "$1\n$2" | sort -V | head -n1)" ]]
}

# Prints all nodejs image versions on docker hub.
function fetch_all_versions() {
    wget -q https://registry.hub.docker.com/v1/repositories/node/tags -O - |\
        sed -e 's/[][]//g' -e 's/"//g' -e 's/ //g' |\
        tr '}' '\n'  |\
        awk -F: '{print $3}' |\
        grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' |\
        sort -t "." -k1,1n -k2,2n -k3,3n
}

# Prints nodejs semantic versions that are newer or same as $1 and older or same as $2.
function get_versions_in_range() {
  v_low="$1"
  v_high="$2"
  # Change grep pattern to include different variants of nodejs:
  # grep -E '^[0-9]+\.[0-9]+\.[0-9]+-.+$' |\
  # See https://github.com/docker-library/docs/tree/master/node#image-variants
  for version in $(fetch_all_versions); do
    if ! verlte "${version}" "${v_high}"; then
      break
    fi
    if verlte "${v_low}" "${version}"; then
      echo "${version}"
    fi
  done
}

REPOSITORY="library/node"
DOCKER_HUB_URL="https://auth.docker.io/token?service=registry.docker.io&scope=repository"
NODE_15_0_1_ETAG="sha256:91aa606ed3ea471af8ea2b5bae066c753b3103ee4a902fbfb4a6db0b7f938d97"
BZL="bazel/container_images.bzl"
BAZEL_TEST="//src/stirling/source_connectors/socket_tracer:openssl_trace_bpf_test"
BAZEL_TEST_FILTER="OpenSSLTraceTest/4.ssl_capture_node_client"

if git diff main --stat | grep "${BZL}"; then
  echo "Reverting ${BZL} to main ..."
  git checkout main "${BZL}"
  git add "${BZL}"
  git commit -m "revert ${BZL}"
fi

VERSION_LOW="0.0.0"
VERSION_HIGH="100.100.100"

if [[ "$1" != "" ]]; then
  VERSION_LOW="$1"
fi

if [[ "$2" != "" ]]; then
  VERSION_HIGH="$2"
fi

for version in $(get_versions_in_range "${VERSION_LOW}" "${VERSION_HIGH}"); do
  echo "==========================================="
  echo "Testing nodejs version=${version} ..."
  TOKEN="$(curl -s "${DOCKER_HUB_URL}:${REPOSITORY}:pull" | jq -r .token)"
  curl_output="$(curl -s -D - -H "Authorization: Bearer ${TOKEN}" \
    -H "Accept: application/vnd.docker.distribution.manifest.v2+json" \
    "https://index.docker.io/v2/${REPOSITORY}/manifests/${version}")"

  # Docker has a rate limit, this might fail.
  echo "curl_output: ${curl_output}"
  node_image_etag="$(echo "${curl_output}" | grep -E '^etag:' | awk '{print $2}' | xargs echo -n)"

  # The output has a trailing line feed (\x0a), remove it.
  node_image_etag="${node_image_etag::-1}"

  # Replace the existing nodejs image with the desired tag.
  sed -i "s/${NODE_15_0_1_ETAG}/${node_image_etag}/" "${BZL}"
  scripts/sudo_bazel_run.sh "${BAZEL_TEST}" -- --gtest_filter="${BAZEL_TEST_FILTER}"

  # Revert the file back
  git checkout "${BZL}"
done
