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

versions_file="$(realpath "${VERSIONS_FILE:?}")"
repo_path=$(pwd)
release_tag=${TAG_NAME##*/v}

# shellcheck source=ci/image_utils.sh
. "${repo_path}/ci/image_utils.sh"
# shellcheck source=ci/artifact_utils.sh
. "${repo_path}/ci/artifact_utils.sh"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name vizier --versions_file "${versions_file}"

build_type="--//k8s:build_type=public"
if [[ $release_tag == *"-"* ]]; then
  build_type="--//k8s:build_type=dev"
fi

push_all_multiarch_images "//k8s/vizier:vizier_images_push" "//k8s/vizier:list_image_bundle" "${release_tag}" "${build_type}"

bazel build --config=stamp -c opt --//k8s:image_version="${release_tag}" \
    --config=stamp "${build_type}" //k8s/vizier:vizier_yamls

yamls_tar="${repo_path}/bazel-bin/k8s/vizier/vizier_yamls.tar"

upload_artifact_to_mirrors "vizier" "${release_tag}" "${yamls_tar}" "vizier_yamls.tar"

# Upload templated YAMLs.
tmp_dir="$(mktemp -d)"
bazel run -c opt //src/utils/template_generator:template_generator -- \
      --base "${yamls_tar}" --version "${release_tag}" --out "${tmp_dir}"
tmpl_path="${tmp_dir}/yamls.tar"
upload_artifact_to_mirrors "vizier" "${release_tag}" "${tmpl_path}" "vizier_template_yamls.tar"

# Update helm chart if it is a release.
if [[ $VERSION != *"-"* ]]; then
  # Update Vizier YAMLS in latest.
  upload_artifact_to_mirrors "vizier" "latest" "${yamls_tar}" "vizier_yamls.tar"

  ./ci/helm_build_release.sh "${release_tag}" "${tmpl_path}"
fi
