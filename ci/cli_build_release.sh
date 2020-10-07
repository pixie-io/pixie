#!/usr/bin/env bash

set -ex

repo_path=$(bazel info workspace)
# shellcheck source=ci/gcs_utils.sh
. "${repo_path}/ci/gcs_utils.sh"

printenv

release_tag=${TAG_NAME##*/v}
arch=x86_64
pkg_prefix="pixie-px-${release_tag}.${arch}"
versions_file="${repo_path}/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"
linux_binary=bazel-bin/src/utils/pixie_cli/px_/px
docker_repo="pixielabs/px"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --build_event_text_file=/tmp/darwin_build --stamp //src/utils/pixie_cli:px_darwin

bazel build -c opt --stamp //src/utils/pixie_cli:px

# Create and push docker image.
bazel run -c opt --stamp //src/utils/pixie_cli:push_px_image

if [[ ! "$release_tag" == *"-"* ]]; then
    # Make tmp directory, because the binary path is a symlink.
    # We need to move the tmp directory to a shared location between the mounted
    # docker volume and the host.
    tmp_dir="$(mktemp -d)"
    cp -RaL "${linux_binary}" "${tmp_dir}"
    mv "${tmp_dir}" /mnt/jenkins/sharedDir
    tmp_subpath="$(echo "${tmp_dir}" | cut -d'/' -f3-)"

    # Create rpm package.
    docker run -i --rm \
           -v "/mnt/jenkins/sharedDir/${tmp_subpath}:/src/" \
           -v "${repo_path}:/image" \
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
           -v "${repo_path}:/image" \
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
    bazel run -c opt --stamp //src/utils/pixie_cli:push_px_image_to_docker

    # Update latest tag.
    docker pull "${docker_repo}:${release_tag}"
    docker tag "${docker_repo}:${release_tag}" "${docker_repo}:latest"
    docker push "${docker_repo}:latest"
fi

write_artifacts_to_gcs() {
    output_path=$1
    mac_binary=$(grep -oP -m 1 '(?<=pl\/).*px_darwin(?=\")' /tmp/darwin_build)
    copy_artifact_to_gcs "$output_path" "$mac_binary" "cli_darwin_amd64_unsigned"
    copy_artifact_to_gcs "$output_path" "$linux_binary" "cli_linux_amd64"

    if [[ ! "$release_tag" == *"-"* ]]; then
        # RPM/DEB only exists for release builds.
        copy_artifact_to_gcs "$output_path" "${repo_path}/${pkg_prefix}.deb" "pixie-px.${arch}.deb"
        copy_artifact_to_gcs "$output_path" "${repo_path}/${pkg_prefix}.rpm" "pixie-px.${arch}.rpm"
    fi
}

output_path="gs://pixie-prod-artifacts/cli/${release_tag}"
write_artifacts_to_gcs "$output_path"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ ! "$release_tag" == *"-"* ]]; then
    output_path="gs://pixie-prod-artifacts/cli/latest"
    write_artifacts_to_gcs "$output_path"
fi
