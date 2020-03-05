#!/usr/bin/env bash

set -ex

printenv

repo_path=$(pwd)
release_tag=${TAG_NAME##*/v}
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --stamp //src/utils/pixie_cli:px_darwin

bazel build -c opt --stamp //src/utils/pixie_cli:px

copy_artifact_to_gcs() {
    output_path="$1"
    binary_path="$2"
    name="$3"

    sha256sum "${binary_path}" | awk '{print $1}' > sha
    gsutil -h 'Content-Disposition:filename=px' cp "${binary_path}" "${output_path}/${name}"
    gsutil cp sha "${output_path}/${name}.sha256"
    gsutil acl ch -u allUsers:READER "${output_path}/${name}"
    gsutil acl ch -u allUsers:READER "${output_path}/${name}.sha256"

}

write_artifacts_to_gcs() {
    output_path=$1
    mac_binary=bazel-bin/src/utils/pixie_cli/darwin_amd64_pure_stripped/px_darwin
    linux_binary=bazel-bin/src/utils/pixie_cli/linux_amd64_stripped/px
    copy_artifact_to_gcs "$output_path" "$mac_binary" "cli_darwin_amd64"
    copy_artifact_to_gcs "$output_path" "$linux_binary" "cli_linux_amd64"
}

output_path="gs://pixie-prod-artifacts/cli/${release_tag}"
write_artifacts_to_gcs "$output_path"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ ! "$release_tag" == *"-"* ]]; then
    output_path="gs://pixie-prod-artifacts/cli/latest"
    write_artifacts_to_gcs "$output_path"
fi
