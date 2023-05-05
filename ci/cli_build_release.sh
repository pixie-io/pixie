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
artifacts_dir="${ARTIFACTS_DIR:-$(mktemp -d)}"
release_tag=${TAG_NAME##*/v}
linux_arch=x86_64
pkg_prefix="pixie-px-${release_tag}.${linux_arch}"

echo "The release tag is: ${release_tag}"
linux_binary=$(bazel cquery //src/pixie_cli:px -c opt --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
darwin_amd64_binary=$(bazel cquery -c opt //src/pixie_cli:px_darwin_amd64 --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
darwin_arm64_binary=$(bazel cquery -c opt //src/pixie_cli:px_darwin_arm64 --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --config=stamp //src/pixie_cli:px_darwin_amd64 //src/pixie_cli:px_darwin_arm64 //src/pixie_cli:px

# Avoid dealing with bazel's symlinks by copying binaries into a temp dir.
binary_dir="$(mktemp -d)"
cp "${linux_binary}" "${binary_dir}"
linux_binary="${binary_dir}/$(basename "${linux_binary}")"
cp "${darwin_amd64_binary}" "${binary_dir}"
darwin_amd64_binary="${binary_dir}/$(basename "${darwin_amd64_binary}")"
cp "${darwin_arm64_binary}" "${binary_dir}"
darwin_arm64_binary="${binary_dir}/$(basename "${darwin_arm64_binary}")"

# Create and push docker image.
bazel run -c opt --config=stamp //src/pixie_cli:push_px_image

if [[ ! "$release_tag" == *"-"* ]]; then
    # Create rpm package.
    podman run -i --rm \
           -v "${binary_dir}:/src/" \
           -v "$(pwd):/image" \
           docker.io/cdrx/fpm-fedora:24 \
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
    podman run -i --rm \
           -v "${binary_dir}:/src/" \
           -v "$(pwd):/image" \
           docker.io/cdrx/fpm-ubuntu:18.04 \
           fpm \
           -f \
           -p "/image/${pkg_prefix}.deb" \
           -s dir \
           -t deb \
           -n pixie-px \
           -v "${release_tag}" \
           --prefix /usr/local/bin \
           px

   # TODO(james): Add push to docker hub/quay.io.
fi

gpg --no-tty --batch --yes --import "${BUILDBOT_GPG_KEY_FILE}"

write_artifacts_to_gcs() {
    output_path=$1
    copy_artifact_to_gcs "${output_path}" "${darwin_amd64_binary}" "cli_darwin_amd64_unsigned"
    copy_artifact_to_gcs "${output_path}" "${darwin_arm64_binary}" "cli_darwin_arm64_unsigned"
    copy_artifact_to_gcs "${output_path}" "${linux_binary}" "cli_linux_amd64"

    if [[ ! "$release_tag" == *"-"* ]]; then
        # RPM/DEB only exists for release builds.
        copy_artifact_to_gcs "${output_path}" "$(pwd)/${pkg_prefix}.deb" "pixie-px.${linux_arch}.deb"
        copy_artifact_to_gcs "${output_path}" "$(pwd)/${pkg_prefix}.rpm" "pixie-px.${linux_arch}.rpm"
    fi
}

sign_artifacts() {
    cp "${linux_binary}" "${artifacts_dir}/cli_linux_amd64"
    cp "$(pwd)/${pkg_prefix}.deb" "${artifacts_dir}/pixie-px.${linux_arch}.deb"
    cp "$(pwd)/${pkg_prefix}.rpm" "${artifacts_dir}/pixie-px.${linux_arch}.rpm"

    pushd "${artifacts_dir}"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "cli_linux_amd64"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "pixie-px.${linux_arch}.deb"
    gpg --no-tty --batch --yes --local-user "${BUILDBOT_GPG_KEY_ID}" --armor --detach-sign "pixie-px.${linux_arch}.rpm"
    popd
}

public="True"
bucket="pixie-dev-public"
if [[ $release_tag == *"-"* ]]; then
  public="False"
  # Use the same bucket for RCs.
fi
output_path="gs://${bucket}/cli/${release_tag}"
write_artifacts_to_gcs "${output_path}"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ $public == "True" ]]; then
    output_path="gs://${bucket}/cli/latest"
    write_artifacts_to_gcs "${output_path}"
    sign_artifacts
fi
