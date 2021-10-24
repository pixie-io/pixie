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

release_tag=${TAG_NAME##*/v}
linux_arch=x86_64
pkg_prefix="pixie-px-${release_tag}.${linux_arch}"
versions_file="${repo_path}/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"
linux_binary=bazel-bin/src/pixie_cli/px_/px
darwin_package_amd64=bazel-bin/src/pixie_cli/px_darwin_amd64_pkg.tar
darwin_package_arm64=bazel-bin/src/pixie_cli/px_darwin_arm64_pkg.tar
docker_repo="pixielabs/px"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

# We write the darwin binary to a tar, because normal cross-compilation of the :px_darwin*
# binary does not write it to a bazel-bin location with a determinable path.
bazel build -c opt --stamp //src/pixie_cli:px_darwin_amd64_pkg
tar -xvf ${darwin_package_amd64}

bazel build -c opt --stamp //src/pixie_cli:px_darwin_arm64_pkg
tar -xvf ${darwin_package_arm64}

bazel build -c opt --stamp //src/pixie_cli:px

# Create and push docker image.
bazel run -c opt --stamp //src/pixie_cli:push_px_image

if [[ ! "$release_tag" == *"-"* ]]; then
    # Make tmp directory, because the binary path is a symlink.
    # We need to move the tmp directory to a shared location between the mounted
    # docker volume and the host.
    tmp_dir="$(mktemp -d)"
    cp -RaL "${linux_binary}" "${tmp_dir}"
    mv "${tmp_dir}" /mnt/jenkins/sharedDir
    tmp_subpath="$(echo "${tmp_dir}" | cut -d'/' -f3-)"
    mkdir -p /mnt/jenkins/sharedDir/image

    # Create rpm package.
    docker run -i --rm \
           -v "/mnt/jenkins/sharedDir/${tmp_subpath}:/src/" \
           -v "/mnt/jenkins/sharedDir/image:/image" \
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
           -v "/mnt/jenkins/sharedDir/${tmp_subpath}:/src/" \
           -v "/mnt/jenkins/sharedDir/image:/image" \
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

write_artifacts_to_gcs() {
    output_path=$1
    copy_artifact_to_gcs "$output_path" "px_darwin_amd64" "cli_darwin_amd64_unsigned"
    copy_artifact_to_gcs "$output_path" "px_darwin_arm64" "cli_darwin_arm64_unsigned"
    copy_artifact_to_gcs "$output_path" "$linux_binary" "cli_linux_amd64"

    if [[ ! "$release_tag" == *"-"* ]]; then
        # RPM/DEB only exists for release builds.
        copy_artifact_to_gcs "$output_path" "/mnt/jenkins/sharedDir/image/${pkg_prefix}.deb" "pixie-px.${linux_arch}.deb"
        copy_artifact_to_gcs "$output_path" "/mnt/jenkins/sharedDir/image/${pkg_prefix}.rpm" "pixie-px.${linux_arch}.rpm"
    fi
}

public="True"
bucket="pixie-dev-public"
if [[ $release_tag == *"-"* ]]; then
  public="False"
  bucket="pixie-prod-artifacts"
fi
output_path="gs://${bucket}/cli/${release_tag}"
write_artifacts_to_gcs "$output_path"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ $public == "True" ]]; then
    output_path="gs://pixie-dev-public/cli/latest"
    write_artifacts_to_gcs "$output_path"
fi
