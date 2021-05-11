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
    echo " -r : Create a prod cloud release."
}

parse_args() {
  while test $# -gt 0; do
      case "$1" in
        -r) RELEASE=true
            shift
            ;;
        *)  usage ;;
      esac
  done
}

parse_args "$@"

repo_path=$(pwd)
image_tag=$(date +%s)

echo "The image tag is: ${image_tag}"

public="False"
if [[ "$RELEASE" == "true" ]]; then
  public="True"
fi

# Build and push images to registry.
bazel run --stamp -c opt --action_env=GOOGLE_APPLICATION_CREDENTIALS --define BUNDLE_VERSION="${image_tag}" \
    --stamp --define public="${public}" //k8s/cloud:cloud_images_push

yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_dev_cloud.yaml"
if [[ "$public" == "True" ]]; then 
  bazel build --stamp -c opt --define BUNDLE_VERSION="${image_tag}" \
    --stamp //k8s/cloud:prod_cloud_yamls
  yaml_path="${repo_path}/bazel-bin/k8s/cloud/pixie_prod_cloud.yaml"
else
  bazel build --stamp -c opt --define BUNDLE_VERSION="${image_tag}" \
    --stamp //k8s/cloud:dev_cloud_yamls
fi

kubectl apply -f "$yaml_path"

