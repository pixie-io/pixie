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

declare -A GO_VERSIONS=(
  ["1.18"]="v1.57.2"
  ["1.19"]="v1.58.3"
  ["1.20"]="v1.58.3"
  ["1.21"]="v1.58.3"
  ["1.22"]="v1.58.3"
)
version=1.0

IMAGES=()

for go_version in "${!GO_VERSIONS[@]}"; do
  tag="ghcr.io/pixie-io/golang_${go_version//./_}_grpc_server_with_buildinfo:$version"
  google_golang_grpc=${GO_VERSIONS[$go_version]}
  echo "Building and pushing image: $tag"
  docker build . --build-arg GO_VERSION="${go_version}" --build-arg GOOGLE_GOLANG_GRPC="${google_golang_grpc}" -t "${tag}"
  docker push "${tag}"
  sha=$(docker inspect --format='{{index .RepoDigests 0}}' "${tag}" | cut -f2 -d'@')
  IMAGES+=("${tag}@${sha}")
done

echo ""
echo "Images pushed!"
echo "IMPORTANT: Now update //bazel/container_images.bzl with the following digest: $sha"
echo "Images:"
for image in "${IMAGES[@]}"; do
  echo "  - $image"
done
