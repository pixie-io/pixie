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

versions_file="$(realpath "${VERSIONS_FILE:?}")"
manifest_updates="${MANIFEST_UPDATES:?}"
repo_path=$(pwd)
release_tag=${TAG_NAME##*/v}

# shellcheck source=ci/image_utils.sh
. "${repo_path}/ci/image_utils.sh"
# shellcheck source=ci/artifact_utils.sh
. "${repo_path}/ci/artifact_utils.sh"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name operator --versions_file "${versions_file}"

# Find the previous bundle version, which this release should replace.
tags=$(git for-each-ref --sort='-*authordate' --format '%(refname:short)' refs/tags \
    | grep "release/operator" | grep -v "\-")

image_repo="gcr.io/pixie-oss/pixie-prod"
image_paths=$(bazel cquery //k8s/operator:image_bundle \
  --//k8s:image_repository="${image_repo}" \
  --//k8s:image_version="${release_tag}" \
  --output=starlark \
  --starlark:expr="'\n'.join(providers(target)['@io_bazel_rules_docker//container:providers.bzl%BundleInfo'].container_images.keys())")
image_path=$(echo "${image_paths}" | grep -v deleter)
deleter_image_path=$(echo "${image_paths}" | grep deleter)

bucket="pixie-dev-public"

channel="stable"
channels="stable,dev"
# The previous version should be the 2nd item in the tags. Since this is a release build,
# the first item in the tag is the current release.
prev_tag=$(echo "$tags" | sed -n '2 p')

if [[ $release_tag == *"-"* ]]; then
  channel="dev"
  channels="dev"
  # The previous version should be the 1st item in the tags. Since this is a non-release build,
  # the first item in the tags is the previous release.
  prev_tag=$(echo "$tags" | sed -n '1 p')
fi

push_all_multiarch_images "//k8s/operator:operator_images_push" "//k8s/operator:list_image_bundle" "${release_tag}" "${image_repo}"

# Build operator bundle for OLM.
tmp_dir="$(mktemp -d)"
kustomize_dir="$(mktemp -d)"
# The bundle can only contain lowercase alphanumeric characters.
bundle_version=$(echo "${release_tag}" | tr '[:upper:]' '[:lower:]')

# An OLM operator bundle containers requires a manifests directory containing the CRDs
# that are used by the operator and a CSV (ClusterServiceVersion) which provides information
# about how the operator should be deployed.
mkdir "${tmp_dir}/manifests"

previous_version=${prev_tag//*\/v/}

kustomize build "$(pwd)/k8s/operator/crd/base" > "${kustomize_dir}/crd.yaml"
kustomize build "$(pwd)/k8s/operator/deployment/base" -o "${kustomize_dir}"

#shellcheck disable=SC2016
faq -f yaml -o yaml --slurp '
  .[0].spec.replaces = $previousName |
  .[0].metadata.name = $name |
  .[0].spec.version = $version |
  .[0].spec.install = {strategy: "deployment", spec:{
  deployments: [{name: .[1].metadata.name, spec: .[1].spec }],
  permissions: [{serviceAccountName: .[3].subjects[0].name, rules: .[2].rules }]}} |
  .[0].spec.install.spec.deployments[0].spec.template.spec.containers[0].image = $image
  | .[0]' \
  "$(pwd)/k8s/operator/bundle/csv.yaml" \
  "${kustomize_dir}/apps_v1_deployment_vizier-operator.yaml" \
  "${kustomize_dir}/rbac.authorization.k8s.io_v1_clusterrole_pixie-operator-role.yaml" \
  "${kustomize_dir}/rbac.authorization.k8s.io_v1_clusterrolebinding_pixie-operator-cluster-binding.yaml" \
  --kwargs version="${release_tag}" --kwargs name="pixie-operator.v${bundle_version}" \
  --kwargs previousName="pixie-operator.v${previous_version}" \
  --kwargs image="${image_path}" > "${tmp_dir}/manifests/csv.yaml"
faq -f yaml -o yaml --slurp '.[0]' "${kustomize_dir}/crd.yaml" > "${tmp_dir}/manifests/crd.yaml"

# Update deleter template image tag.
#shellcheck disable=SC2016
faq -f yaml -o yaml --slurp '.[0].spec.template.spec.containers[0].image = $imagePath | .[0]' \
  "$(pwd)/k8s/operator/helm/templates/deleter.yaml" \
  --kwargs imagePath="${deleter_image_path}" > "$(pwd)/k8s/operator/helm/templates/deleter_tmp.yaml"
mv "$(pwd)/k8s/operator/helm/templates/deleter_tmp.yaml" "$(pwd)/k8s/operator/helm/templates/deleter.yaml"

# Build and push bundle.
cd "${tmp_dir}"
bundle_image="gcr.io/pixie-oss/pixie-prod/operator/bundle:${release_tag}"
index_image="gcr.io/pixie-oss/pixie-prod/operator/bundle_index:0.0.1"

docker buildx create --name builder --driver docker-container --bootstrap
docker buildx use builder

opm alpha bundle generate --package pixie-operator --channels "${channels}" --default "${channel}" --directory manifests
docker buildx build --platform linux/amd64,linux/arm64 -t "${bundle_image}" --push -f bundle.Dockerfile .
opm index add --bundles "${bundle_image}" --from-index "${index_image}" --tag "${index_image}"  --generate --out-dockerfile="${tmp_dir}/index.Dockerfile" -u docker
docker buildx build --platform linux/amd64,linux/arm64 -t "${index_image}" --push -f "${tmp_dir}/index.Dockerfile" .

cd "${repo_path}"

# Upload templated YAMLs.
output_path="gs://${bucket}/operator/${release_tag}"
bazel build //k8s/operator:operator_templates
yamls_tar="${repo_path}/bazel-bin/k8s/operator/operator_templates.tar"

upload_artifact_to_mirrors "operator" "${release_tag}" "${yamls_tar}" "operator_template_yamls.tar" AT_CONTAINER_SET_TEMPLATE_YAMLS

./ci/operator_helm_build_release.sh "${release_tag}"

create_manifest_update "operator" "${release_tag}" > "${manifest_updates}"
