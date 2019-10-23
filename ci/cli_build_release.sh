#!/usr/bin/env bash

set -ex

printenv

repo_path=$(pwd)
release_tag=$(echo $TAG_NAME | sed 's|release/cli-\(v.*\)|\1|')
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --stamp //src/utils/pixie_cli:pixie_mac

bazel build -c opt --stamp //src/utils/pixie_cli:pixie

mac_binary=bazel-bin/src/utils/pixie_cli/darwin_amd64_pure_stripped/pixie_mac
linux_binary=bazel-bin/src/utils/pixie_cli/linux_amd64_stripped/pixie
output_path="gs://pixie-prod-artifacts/cli/${release_tag}"

sha256sum ${mac_binary} > sha
gsutil cp ${mac_binary} "${output_path}/cli_darwin_amd64"
gsutil cp sha "${output_path}/cli_darwin_amd64.sha256"

sha256sum ${linux_binary} > sha
gsutil cp ${linux_binary} "${output_path}/cli_linux_amd64"
gsutil cp sha "${output_path}/cli_linux_amd64.sha256"
