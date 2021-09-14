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
    echo "Usage: $0 <version> <yaml_tar>"
    echo "Example: $0 0.1.2 /tmp/yamls.tar"
}

parse_args() {
  if [ $# -lt 2 ]; then
    usage
  fi

  VERSION=$1
  YAML_TAR=$2
}

parse_args "$@"

helm_gcs_bucket="pixie-helm-charts"
tmp_path="/tmp/helm-${VERSION}"
tmp_dir="$(mktemp -d)"
repo_path=$(pwd)

mkdir -p "${tmp_path}"
# A Helm chart contains two main items:
# 1. Chart.yaml which specifies the version and name of the chart.
# 2. `templates` directory, which contains the template YAMLs that should be filled out.

# Create Chart.yaml for this release.
echo "apiVersion: v2
name: pixie-chart
type: application
version: ${VERSION}" > "${tmp_path}/Chart.yaml"

# Extract templated YAMLs.
tar xvf "${YAML_TAR}" -C "${tmp_dir}"
mv "${tmp_dir}/pixie_yamls" "${tmp_path}/templates"

# Add crds. Helm ensures that these crds are deployed before the templated YAMLs.
bazel build //k8s/vizier_deps:vizier_deps_crds
mv "${repo_path}/bazel-bin/k8s/vizier_deps/vizier_deps_crds.tar" "${tmp_dir}"
tar xvf "${tmp_dir}/vizier_deps_crds.tar" -C "${tmp_path}"

# Fetch all of the current charts in GCS, because generating the index needs all pre-existing tar versions present.
mkdir -p "${tmp_dir}/${helm_gcs_bucket}"
gsutil rsync "gs://${helm_gcs_bucket}" "${tmp_dir}/${helm_gcs_bucket}"

# Generates tgz for the new release helm chart.
helm package "${tmp_path}" -d "${tmp_dir}/${helm_gcs_bucket}"

# Update the index file.
helm repo index "${tmp_dir}/${helm_gcs_bucket}" --url "https://${helm_gcs_bucket}.storage.googleapis.com"

# Upload the new index and tar to gcs by syncing. This will help keep the timestamps for pre-existing tars the same.
gsutil rsync "${tmp_dir}/${helm_gcs_bucket}" "gs://${helm_gcs_bucket}"
