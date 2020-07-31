#!/usr/bin/env bash

set -ex

repo_path=$(bazel info workspace)
# shellcheck source=ci/gcs_utils.sh
. "${repo_path}/ci/gcs_utils.sh"

printenv

release_tag=${TAG_NAME##*/v}
versions_file="${repo_path}/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --build_event_text_file=/tmp/darwin_build --stamp //src/utils/pixie_cli:px_darwin

bazel build -c opt --stamp //src/utils/pixie_cli:px

write_artifacts_to_gcs() {
    output_path=$1
    mac_binary=$(grep -oP -m 1 '(?<=pl\/).*px_darwin(?=\")' /tmp/darwin_build)
    linux_binary=bazel-bin/src/utils/pixie_cli/px_/px
    copy_artifact_to_gcs "$output_path" "$mac_binary" "cli_darwin_amd64_unsigned"
    copy_artifact_to_gcs "$output_path" "$linux_binary" "cli_linux_amd64"
}

output_path="gs://pixie-prod-artifacts/cli/${release_tag}"
write_artifacts_to_gcs "$output_path"
# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ ! "$release_tag" == *"-"* ]]; then
    output_path="gs://pixie-prod-artifacts/cli/latest"
    write_artifacts_to_gcs "$output_path"
fi
