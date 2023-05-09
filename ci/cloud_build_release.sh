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

artifacts_dir="${ARTIFACTS_DIR:?}"

repo_path=$(pwd)

release_tag=${TAG_NAME##*/v}

release="true"
if [[ "${release_tag}" == *"-"* ]]; then
  release="false"
fi

echo "The image tag is: ${release_tag}"

bazel run --config=stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --//k8s:image_version="${release_tag}" \
    --//k8s:build_type=public //k8s/cloud:cloud_images_push

all_licenses_opts=("//tools/licenses:all_licenses" "--action_env=GOOGLE_APPLICATION_CREDENTIALS" "--remote_download_outputs=toplevel")
all_licenses_path="$(bazel cquery "${all_licenses_opts[@]}"  --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)"
bazel build "${all_licenses_opts[@]}"

gsutil cp "${all_licenses_path}" "gs://pixie-dev-public/oss-licenses/${release_tag}.json"
if [[ "${release}" == "true" ]]; then
  gsutil cp "${all_licenses_path}" "gs://pixie-dev-public/oss-licenses/latest.json"
fi

# Write YAMLs + image paths to a tar file to support easy deployment.
mkdir -p "${repo_path}/pixie_cloud/yamls"
image_list_file="${repo_path}/pixie_cloud/cloud_image_list.txt"

kustomize build "k8s/cloud_deps/public/" > "${repo_path}/pixie_cloud/yamls/cloud_deps.yaml"
kustomize build "k8s/cloud_deps/base/elastic/operator" > "${repo_path}/pixie_cloud/yamls/cloud_deps_elastic_operator.yaml"
kustomize build "k8s/cloud/public/" > "${repo_path}/pixie_cloud/yamls/cloud.yaml"

deploy_yamls=(
  "${repo_path}/pixie_cloud/yamls/cloud_deps.yaml"
  "${repo_path}/pixie_cloud/yamls/cloud_deps_elastic_operator.yaml"
  "${repo_path}/pixie_cloud/yamls/cloud.yaml"
)

bazel run @com_github_mikefarah_yq_v4//:v4 -- '..|.image?|select(.|type == "!!str")' -o=json "${deploy_yamls[@]}" | sort | uniq > "${image_list_file}"

cd "${repo_path}"
tar -czvf "${repo_path}/pixie_cloud.tar.gz" "pixie_cloud"
gsutil cp "${repo_path}/pixie_cloud.tar.gz" "gs://pixie-dev-public/cloud/${release_tag}/pixie_cloud.tar.gz"
if [[ "${release}" == "true" ]]; then
  gsutil cp "${repo_path}/pixie_cloud.tar.gz" "gs://pixie-dev-public/cloud/latest/pixie_cloud.tar.gz"
fi

sha256sum "${repo_path}/pixie_cloud.tar.gz" | awk '{print $1}' > sha
cp "${repo_path}/pixie_cloud.tar.gz" "${artifacts_dir}/pixie_cloud.tar.gz"
cp sha "${artifacts_dir}/pixie_cloud.tar.gz.sha256"
