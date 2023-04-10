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

repo_path=$(bazel info workspace)
# shellcheck source=ci/gcs_utils.sh
. "${repo_path}/ci/gcs_utils.sh"

printenv

versions_file="$(realpath "${VERSIONS_FILE:?}")"
release_tag=${TAG_NAME##*/v}
linux_arch=x86_64
pkg_prefix="pixie-px-${release_tag}.${linux_arch}"

echo "The release tag is: ${release_tag}"
linux_binary=$(bazel cquery //src/pixie_cli:px -c opt --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
darwin_amd64_binary=$(bazel cquery -c opt //src/pixie_cli:px_darwin_amd64 --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
darwin_arm64_binary=$(bazel cquery -c opt //src/pixie_cli:px_darwin_arm64 --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
docker_repo="pixielabs/px"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --stamp //src/pixie_cli:px_darwin_amd64 //src/pixie_cli:px_darwin_arm64 //src/pixie_cli:px

# Create and push docker image.
bazel run -c opt --stamp //src/pixie_cli:push_px_image

if [[ ! "$release_tag" == *"-"* ]]; then
    # Make tmp directory, because the binary path is a symlink.
    # We need to move the tmp directory to a shared location between the mounted
    # docker volume and the host.
    tmp_dir="$(mktemp -d)"
    cp -RaL "${linux_binary}" "${tmp_dir}"
    mv "${tmp_dir}" /mnt/disks/jenkins/sharedDir
    tmp_subpath="$(echo "${tmp_dir}" | cut -d'/' -f3-)"
    mkdir -p /mnt/disks/jenkins/sharedDir/image

    # Create rpm package.
    docker run -i --rm \
           -v "/mnt/disks/jenkins/sharedDir/${tmp_subpath}:/src/" \
           -v "/mnt/disks/jenkins/sharedDir/image:/image" \
           cdrx/fpm-fedora:24 \
           fpm \
           -f \
           -p "/image/${pkg_prefix}.rpm" \
           -s dir \
           -t rpm \
           -n pixie-px \
           -v "${release_tag}" \
           --prefix /usr/local/bin \
           px

    # Create deb package.
    docker run -i --rm \
           -v "/mnt/disks/jenkins/sharedDir/${tmp_subpath}:/src/" \
           -v "/mnt/disks/jenkins/sharedDir/image:/image" \
           cdrx/fpm-ubuntu:18.04 \
           fpm \
           -f \
           -p "/image/${pkg_prefix}.deb" \
           -s dir \
           -t deb \
           -n pixie-px \
           -v "${release_tag}" \
           --prefix /usr/local/bin \
           px

    # Push officially releases to docker hub.
    bazel run -c opt --stamp //src/pixie_cli:push_px_image_to_docker

    # Update latest tag.
    docker pull "${docker_repo}:${release_tag}"
    docker tag "${docker_repo}:${release_tag}" "${docker_repo}:latest"
    docker push "${docker_repo}:latest"
fi

gpg --no-tty --batch --yes --import "${BUILDBOT_GPG_KEY_FILE}"

write_artifacts_to_gcs() {
    output_path=$1
    copy_artifact_to_gcs "${output_path}" "${darwin_amd64_binary}" "cli_darwin_amd64_unsigned"
    copy_artifact_to_gcs "${output_path}" "${darwin_arm64_binary}" "cli_darwin_arm64_unsigned"
    copy_artifact_to_gcs "${output_path}" "${linux_binary}" "cli_linux_amd64"

    if [[ ! "$release_tag" == *"-"* ]]; then
        # RPM/DEB only exists for release builds.
        copy_artifact_to_gcs "${output_path}" "/mnt/disks/jenkins/sharedDir/image/${pkg_prefix}.deb" "pixie-px.${linux_arch}.deb"
        copy_artifact_to_gcs "${output_path}" "/mnt/disks/jenkins/sharedDir/image/${pkg_prefix}.rpm" "pixie-px.${linux_arch}.rpm"
    fi
}

write_artifacts_to_gh() {
    gh release create "${TAG_NAME}" --repo=pixie-io/pixie --notes "Pixie CLI Release"

    tmp_dir="$(mktemp -d)"

    cp "${linux_binary}" "${tmp_dir}/cli_linux_amd64"
    cp "/mnt/disks/jenkins/sharedDir/image/${pkg_prefix}.deb" "${tmp_dir}/pixie-px.${linux_arch}.deb"
    cp "/mnt/disks/jenkins/sharedDir/image/${pkg_prefix}.rpm" "${tmp_dir}/pixie-px.${linux_arch}.rpm"

    pushd "${tmp_dir}"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "cli_linux_amd64"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "pixie-px.${linux_arch}.deb"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "pixie-px.${linux_arch}.rpm"

    gh release upload "${TAG_NAME}" --repo=pixie-io/pixie "cli_linux_amd64" "cli_linux_amd64.asc" "pixie-px.${linux_arch}.deb" "pixie-px.${linux_arch}.deb.asc" "pixie-px.${linux_arch}.rpm" "pixie-px.${linux_arch}.rpm.asc"
    popd
}

public="True"
bucket="pixie-dev-public"
if [[ $release_tag == *"-"* ]]; then
  public="False"
  bucket="pixie-prod-artifacts"
fi
output_path="gs://${bucket}/cli/${release_tag}"
write_artifacts_to_gcs "${output_path}"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ $public == "True" ]]; then
    output_path="gs://pixie-dev-public/cli/latest"
    write_artifacts_to_gcs "${output_path}"
    write_artifacts_to_gh
fi
