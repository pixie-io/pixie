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
            prod="True"
            shift
            ;;
        -p) PUBLIC=true
            shift
            ;;
        *)  usage ;;
      esac
  done
}

prod="False"

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
  bazel run --stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --define BUNDLE_VERSION="${image_tag}" \
      --define public=True //k8s/cloud:cloud_images_push

  gsutil cp "${repo_path}/bazel-bin/tools/licenses/all_licenses.json" "gs://pixie-dev-public/oss-licenses/${image_tag}.json"
  gsutil cp "${repo_path}/bazel-bin/tools/licenses/all_licenses.json" "gs://pixie-dev-public/oss-licenses/latest.json"

  bazel run --stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --define BUNDLE_VERSION="${image_tag}" \
      //src/cloud/plugin/load_db:push_plugin_db_updater_image
  bazel run --stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --define BUNDLE_VERSION="latest" \
      //src/cloud/plugin/load_db:push_plugin_db_updater_image
  exit 0
fi

bazel run --stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --define BUNDLE_VERSION="${image_tag}" \
    --define prod="${prod}" //k8s/cloud:cloud_images_push

yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_staging_cloud.yaml"
# Build prod YAMLs.
if [[ "$RELEASE" == "true" ]]; then
  yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_prod_cloud.yaml"
  bazel build --stamp -c opt --define BUNDLE_VERSION="${image_tag}" //k8s/cloud:prod_cloud_yamls
else # Build staging YAMLs.
  bazel build --stamp -c opt --define BUNDLE_VERSION="${image_tag}" //k8s/cloud:staging_cloud_yamls
fi

kubectl apply -f "$yaml_path"
