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

set -ex

printenv

repo_path=$(pwd)
release_tag=${TAG_NAME##*/v}
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name vizier --versions_file "${versions_file}"

build_type="--//k8s:build_type=public"
bucket="pixie-dev-public"
extra_bazel_args=()
if [[ $release_tag == *"-"* ]]; then
  build_type="--//k8s:build_type=dev"
  # TODO(vihang/michelle): Revisit this bucket.
  bucket="pixie-prod-artifacts"
fi

output_path="gs://${bucket}/vizier/${release_tag}"
latest_output_path="gs://${bucket}/vizier/latest"

push_images_for_arch() {
  arch="$1"
  bazel run --stamp -c opt --//k8s:image_version="${release_tag}-${arch}" \
      --config="${arch}_sysroot" \
      --stamp "${build_type}" //k8s/vizier:vizier_images_push "${extra_bazel_args[@]}" > /dev/null
}

push_images_for_arch "x86_64"
push_images_for_arch "aarch64"

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

while read -r image;
do
  push_multiarch_image "${image}"
done < <(bazel run --stamp -c opt --//k8s:image_version="${release_tag}" \
         --stamp "${build_type}" //k8s/vizier:list_image_bundle "${extra_bazel_args[@]}")

bazel build --stamp -c opt --//k8s:image_version="${release_tag}" \
    --stamp "${build_type}" //k8s/vizier:vizier_yamls "${extra_bazel_args[@]}"

output_path="gs://${bucket}/vizier/${release_tag}"
yamls_tar="${repo_path}/bazel-bin/k8s/vizier/vizier_yamls.tar"

sha256sum "${yamls_tar}" | awk '{print $1}' > sha
gsutil cp "${yamls_tar}" "${output_path}/vizier_yamls.tar"
gsutil cp sha "${output_path}/vizier_yamls.tar.sha256"

# Upload templated YAMLs.
tmp_dir="$(mktemp -d)"
bazel run -c opt //src/utils/template_generator:template_generator -- \
      --base "${yamls_tar}" --version "${release_tag}" --out "${tmp_dir}"
tmpl_path="${tmp_dir}/yamls.tar"
sha256sum "${tmpl_path}" | awk '{print $1}' > tmplSha
gsutil cp "${tmpl_path}" "${output_path}/vizier_template_yamls.tar"
gsutil cp tmplSha "${output_path}/vizier_template_yamls.tar.sha256"

# Update helm chart if it is a release.
if [[ $public == "True" ]]; then
  # Update Vizier YAMLS in latest.
  gsutil cp "${yamls_tar}" "${latest_output_path}/vizier_yamls.tar"
  gsutil cp sha "${latest_output_path}/vizier_yamls.tar.sha256"

  ./ci/helm_build_release.sh "${release_tag}" "${tmpl_path}"
fi
