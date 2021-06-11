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

public="True"
bucket="pixie-dev-public"
if [[ $release_tag == *"-"* ]]; then
  public="False"
  bucket="pixie-prod-artifacts"
fi

output_path="gs://${bucket}/vizier/${release_tag}"

bazel run --stamp -c opt --define BUNDLE_VERSION="${release_tag}" \
    --stamp --define public="${public}" //k8s/vizier:vizier_images_push
bazel build --stamp -c opt --define BUNDLE_VERSION="${release_tag}" \
    --stamp --define public="${public}" //k8s/vizier:vizier_yamls

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
  ./ci/helm_build_release.sh "${release_tag}" "${tmpl_path}"
fi
