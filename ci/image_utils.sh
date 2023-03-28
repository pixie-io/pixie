#!/usr/bin/env bash

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

push_images_for_arch() {
  arch="$1"
  image_rule="$2"
  release_tag="$3"
  build_type="$4"
  bazel_args="$5"

  bazel run --stamp -c opt --//k8s:image_version="${release_tag}-${arch}" \
      --config="${arch}_sysroot" \
      --stamp "${build_type}" "${image_rule}" "${bazel_args}" > /dev/null
}

push_multiarch_image() {
  multiarch_image="$1"
  x86_image="${multiarch_image}-x86_64"
  aarch64_image="${multiarch_image}-aarch64"
  echo "Building ${multiarch_image} manifest"
  # If the multiarch manifest list already exists locally, remove it before building a new one.
  # otherwise, the docker manifest create step will fail because it can't amend manifests to an existing image.
  # We could use the --amend flag to `manifest create` but it doesn't seem to overwrite existing images with the same tag,
  # instead it seems to just ignore images that already exist in the local manifest.
  docker manifest rm "${multiarch_image}" || true
  docker manifest create "${multiarch_image}" "${x86_image}" "${aarch64_image}"
  docker manifest push "${multiarch_image}"
}

push_all_multiarch_images() {
  image_rule="$1"
  image_list_rule="$2"
  release_tag="$3"
  build_type="$4"
  bazel_args="$5"

  push_images_for_arch "x86_64" "${image_rule}" "${release_tag}" "${build_type}" "${bazel_args}"
  push_images_for_arch "aarch64" "${image_rule}" "${release_tag}" "${build_type}" "${bazel_args}"

  while read -r image;
  do
    push_multiarch_image "${image}"
  done < <(bazel run --stamp -c opt --//k8s:image_version="${release_tag}" \
          --stamp "${build_type}" "${image_list_rule}" "${bazel_args}")
}
