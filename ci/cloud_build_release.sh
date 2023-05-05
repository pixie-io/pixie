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

usage() {
    echo "Usage: $0 [-r]"
    echo " -r : Create a prod proprietary cloud release."
    echo " -p : Create a public cloud release."
}

parse_args() {
  while test $# -gt 0; do
      case "$1" in
        -r) RELEASE=true
            shift
            ;;
        -p) PUBLIC=true
            shift
            ;;
        *)  usage ;;
      esac
  done
}

parse_args "$@"

repo_path=$(pwd)

if [[ -z "${TAG_NAME}" ]]; then
  image_tag=$(date +%s)
else
  image_tag=$(echo "${TAG_NAME}" | awk -F/ '{print $NF}')
fi

echo "The image tag is: ${image_tag}"

# We are building the OSS images/YAMLs. In this case, we only want to push the images but not deploy the YAMLs.
if [[ "$PUBLIC" == "true" ]]; then
  bazel run --config=stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --//k8s:image_version="${image_tag}" \
      --//k8s:build_type=public //k8s/cloud:cloud_images_push

  bazel build //tools/licenses:all_licenses --action_env=GOOGLE_APPLICATION_CREDENTIALS

  gsutil cp "${repo_path}/bazel-bin/tools/licenses/all_licenses.json" "gs://pixie-dev-public/oss-licenses/${image_tag}.json"
  gsutil cp "${repo_path}/bazel-bin/tools/licenses/all_licenses.json" "gs://pixie-dev-public/oss-licenses/latest.json"

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
  gsutil cp "${repo_path}/pixie_cloud.tar.gz" "gs://pixie-dev-public/cloud/${image_tag}/pixie_cloud.tar.gz"
  gsutil cp "${repo_path}/pixie_cloud.tar.gz" "gs://pixie-dev-public/cloud/latest/pixie_cloud.tar.gz"

  exit 0
fi

bazel run --config=stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --//k8s:image_version="${image_tag}" \
    --//k8s:build_type=proprietary //k8s/cloud:cloud_images_push

yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_staging_cloud.yaml"
# Build prod YAMLs.
if [[ "$RELEASE" == "true" ]]; then
  yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_prod_cloud.yaml"
  bazel build --config=stamp -c opt --//k8s:image_version="${image_tag}" //k8s/cloud:pixie_prod_cloud
else # Build staging YAMLs.
  bazel build --config=stamp -c opt --//k8s:image_version="${image_tag}" //k8s/cloud:pixie_staging_cloud
fi

kubectl apply -f "$yaml_path"
