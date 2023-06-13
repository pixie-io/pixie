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
index_file="${INDEX_FILE:?}"
gh_repo="${GH_REPO:?}"

helm_gcs_bucket="pixie-operator-charts"
if [[ $VERSION == *"-"* ]]; then
  helm_gcs_bucket="pixie-operator-charts-dev"
fi

repo_path=$(pwd)
# shellcheck source=ci/artifact_utils.sh
. "${repo_path}/ci/artifact_utils.sh"
helm_path="${repo_path}/k8s/operator/helm"

# Create Chart.yaml for this release for Helm3.
echo "apiVersion: v2
name: pixie-operator-chart
type: application
version: ${VERSION}" > "${helm_path}/Chart.yaml"

# Add crds. Helm ensures that these crds are deployed before the templated YAMLs.
cp "${repo_path}/k8s/operator/crd/base/px.dev_viziers.yaml" "${helm_path}/crds/vizier_crd.yaml"

# Updates templates with Helm-specific template functions.
helm_tmpl_checks="$(cat "${repo_path}/k8s/operator/helm/olm_template_checks.tmpl")"
sed -i "1c${helm_tmpl_checks}" "${repo_path}/k8s/operator/helm/templates/00_olm.yaml"
rm "${repo_path}/k8s/operator/helm/olm_template_checks.tmpl"

# Fetch all of the current charts in GCS, because generating the index needs all pre-existing tar versions present.
mkdir -p "${tmp_dir}/${helm_gcs_bucket}"
gsutil rsync "gs://${helm_gcs_bucket}" "${tmp_dir}/${helm_gcs_bucket}"

# Generates tgz for the new release helm3 chart.
helm package "${helm_path}" -d "${tmp_dir}/${helm_gcs_bucket}"

# Create release for Helm2.
mkdir "${helm_path}2"

# Create Chart.yaml for this release for Helm2.
echo "apiVersion: v1
name: pixie-operator-helm2-chart
type: application
version: ${VERSION}" > "${helm_path}2/Chart.yaml"

cp -r "${helm_path}/templates" "${helm_path}2/templates"
cp "${helm_path}/values.yaml" "${helm_path}2/values.yaml"

# Generates tgz for the new release helm3 chart.
helm package "${helm_path}2" -d "${tmp_dir}/${helm_gcs_bucket}"

# Update the index file.
helm repo index "${tmp_dir}/${helm_gcs_bucket}" --url "https://${helm_gcs_bucket}.storage.googleapis.com"

upload_artifact_to_mirrors "operator" "${VERSION}" "${tmp_dir}/${helm_gcs_bucket}/pixie-operator-chart-${VERSION}.tgz" "pixie-operator-chart-${VERSION}.tgz"

# Upload the new index and tar to gcs by syncing. This will help keep the timestamps for pre-existing tars the same.
gsutil rsync "${tmp_dir}/${helm_gcs_bucket}" "gs://${helm_gcs_bucket}"

# Generate separate index file for GH.
mkdir -p "${tmp_dir}/gh_helm_chart"
helm package "${helm_path}" -d "${tmp_dir}/gh_helm_chart"
# Pull index file.
curl https://artifacts.px.dev/helm_charts/operator/index.yaml -o old_index.yaml
# Update the index file.
helm repo index "${tmp_dir}/gh_helm_chart" --merge old_index.yaml --url "https://github.com/${gh_repo}/releases/download/release%2Foperator%2Fv${VERSION}"
mv "${tmp_dir}/gh_helm_chart/index.yaml" "${index_file}"
