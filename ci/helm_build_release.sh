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

tmp_path="/tmp/helm-${VERSION}"
tmp_dir="$(mktemp -d)"
artifacts_dir="${ARTIFACTS_DIR:?}"
index_file="${INDEX_FILE:?}"
gh_repo="${GH_REPO:?}"

mkdir -p "${tmp_path}"
# A Helm chart contains two main items:
# 1. Chart.yaml which specifies the version and name of the chart.
# 2. `templates` directory, which contains the template YAMLs that should be filled out.

# Create Chart.yaml for this release.
echo "apiVersion: v2
name: vizier-chart
type: application
version: ${VERSION}" > "${tmp_path}/Chart.yaml"

# Extract templated YAMLs.
tar xvf "${YAML_TAR}" -C "${tmp_dir}"
mv "${tmp_dir}/pixie_yamls" "${tmp_path}/templates"

mkdir -p "${tmp_dir}/charts"

# Generates tgz for the new release helm chart.
helm package "${tmp_path}" -d "${tmp_dir}/charts"

cp "${tmp_dir}/charts/vizier-chart-${VERSION}.tgz" "${artifacts_dir}/vizier-chart-${VERSION}.tgz"
sha256sum "${tmp_dir}/charts/vizier-chart-${VERSION}.tgz" | awk '{print $1}' > sha
cp sha "${artifacts_dir}/vizier-chart-${VERSION}.tgz.sha256"

# Pull index file.
curl https://artifacts.px.dev/helm_charts/vizier/index.yaml -o old_index.yaml
# Update the index file.
helm repo index "${tmp_dir}/charts" --merge old_index.yaml --url "https://github.com/${gh_repo}/releases/download/release%2Fvizier%2Fv${VERSION}"
mv "${tmp_dir}/charts/index.yaml" "${index_file}"
