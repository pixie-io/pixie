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

declare -A GO_IMAGE_DIGEST_MAP=(
  ["1.18-alpine@sha256:77f25981bd57e60a510165f3be89c901aec90453fd0f1c5a45691f6cb1528807"]="v0.35.0"
  ["1.19-alpine@sha256:0ec0646e208ea58e5d29e558e39f2e59fccf39b7bda306cb53bbaff91919eca5"]="v0.35.0"
  ["1.20-alpine@sha256:e47f121850f4e276b2b210c56df3fda9191278dd84a3a442bfe0b09934462a8f"]="v0.35.0"
  ["1.21-alpine@sha256:2414035b086e3c42b99654c8b26e6f5b1b1598080d65fd03c7f499552ff4dc94"]="v0.35.0"
  ["1.22-alpine@sha256:1699c10032ca2582ec89a24a1312d986a3f094aed3d5c1147b19880afe40e052"]="v0.35.0"
)
version=1.0

IMAGES=()

for go_image_digest in "${!GO_IMAGE_DIGEST_MAP[@]}"; do
  tag="ghcr.io/pixie-io/golang_${go_image_digest//./_}_https_server_with_buildinfo:$version"
  x_net_version=${GO_IMAGE_DIGEST_MAP[$go_image_digest]}
  echo "Building and pushing image: $tag"
  docker build . --build-arg GO_IMAGE_DIGEST="${go_image_digest}" --build-arg GOLANG_X_NET="${x_net_version}" -t "${tag}"
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
