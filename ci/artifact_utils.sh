#!/bin/bash

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

gh_artifacts_dir="${ARTIFACTS_DIR}"
workspace=$(git rev-parse --show-toplevel)
mirrors_file="${workspace}/ci/artifact_mirrors.yaml"

BUILDBOT_GPG_KEY_ID="${BUILDBOT_GPG_KEY_ID:?}"

upload_artifact_to_mirrors() {
  component="$1"
  version="$2"
  artifact_path="$3"
  artifact_name="$4"

  # Create SHA and signature files for the artifact.
  sha256sum "${artifact_path}" | awk '{print $1}' > "${artifact_path}.sha256"
  gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "${artifact_path}"

  while read -r mirror; do
    mirror_def="$(yq '.[] | select(.name == "'"${mirror}"'")' "${mirrors_file}")"
    mirror_type="$(echo "${mirror_def}" | yq '.type')"
    case "${mirror_type}" in
      gh-releases)
        if [[ "${version}" == "latest" ]]; then
          # gh-releases have no consistent latest across multiple components,
          # so we only upload "latest" artifacts to GCS.
          continue;
        fi
        gh_release_mirror "${artifact_path}" "${artifact_name}"
        ;;
      gcs)
        bucket="$(echo "${mirror_def}" | yq '.bucket' )"
        path_format="$(echo "${mirror_def}" | yq '.path_format')"
        path="$(echo "${path_format}" | env - \
          component="${component}" \
          version="${version}" \
          artifact_name="${artifact_name}" \
          bash -c "echo ${path_format}")"
        upload_to_gcs "${artifact_path}" "${bucket}" "${path}"
        ;;
    esac
  done < <(yq '.[].name' "${mirrors_file}")
}

gh_release_mirror() {
  artifact_path="$1"
  artifact_name="$2"
  # This is expected to run in a github action with ARTIFACTS_DIR defined
  if [[ -z "${gh_artifacts_dir}" ]]; then
    echo "Must run in github actions to use gh-releases mirror"
    exit 1
  fi

  cp "${artifact_path}" "${gh_artifacts_dir}/${artifact_name}"
  cp "${artifact_path}.sha256" "${gh_artifacts_dir}/${artifact_name}.sha256"
  cp "${artifact_path}.asc" "${gh_artifacts_dir}/${artifact_name}.asc"
}

upload_to_gcs() {
  artifact_path="$1"
  gcs_bucket="$2"
  gcs_path="$3"
  read -r -a gcs_aritfact_opts <<<"${GCS_ARTIFACT_OPTS}"

  gsutil "${gcs_aritfact_opts[@]}" cp "${artifact_path}" "gs://${gcs_bucket}/${gcs_path}"
  gsutil cp "${artifact_path}.sha256" "gs://${gcs_bucket}/${gcs_path}.sha256"
  gsutil cp "${artifact_path}.asc" "gs://${gcs_bucket}/${gcs_path}.asc"
}
