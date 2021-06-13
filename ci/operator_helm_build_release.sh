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

usage() {
    echo "Usage: $0 <version>"
    echo "Example: $0 0.1.2"
}

parse_args() {
  if [ $# -lt 1 ]; then
    usage
  fi

  VERSION=$1
}

parse_args "$@"
tmp_dir="$(mktemp -d)"

helm_gcs_bucket="pixie-operator-charts"
repo_path=$(pwd)
helm_path="${repo_path}/k8s/operator/helm"

# Create Chart.yaml for this release.
echo "apiVersion: v2
name: pixie-operator-chart
type: application
version: ${VERSION}" > "${helm_path}/Chart.yaml"

# Add crds. Helm ensures that these crds are deployed before the templated YAMLs.
bazel build //k8s/vizier_deps:vizier_deps_crds
mv "${repo_path}/bazel-bin/k8s/vizier_deps/vizier_deps_crds.tar" "${tmp_dir}"
tar xvf "${tmp_dir}/vizier_deps_crds.tar" -C "${helm_path}"
cp "${repo_path}/k8s/operator/crd/base/px.dev_viziers.yaml" "${helm_path}/crds/vizier_crd.yaml"

# Fetch all of the current charts in GCS, because generating the index needs all pre-existing tar versions present.
mkdir -p "${tmp_dir}/${helm_gcs_bucket}"
gsutil rsync "gs://${helm_gcs_bucket}" "${tmp_dir}/${helm_gcs_bucket}"

# Generates tgz for the new release helm chart.
helm package "${helm_path}" -d "${tmp_dir}/${helm_gcs_bucket}"

# Update the index file.
helm repo index "${tmp_dir}/${helm_gcs_bucket}" --url "https://${helm_gcs_bucket}.storage.googleapis.com"

# Upload the new index and tar to gcs by syncing. This will help keep the timestamps for pre-existing tars the same.
gsutil rsync "${tmp_dir}/${helm_gcs_bucket}" "gs://${helm_gcs_bucket}"


