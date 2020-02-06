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

mac_binary=bazel-bin/src/utils/pixie_cli/darwin_amd64_pure_stripped/px_darwin
linux_binary=bazel-bin/src/utils/pixie_cli/linux_amd64_stripped/px
output_path="gs://pixie-prod-artifacts/cli/${release_tag}"

sha256sum ${mac_binary} | awk '{print $1}' > sha
gsutil -h 'Content-Disposition:filename=px' cp ${mac_binary} "${output_path}/cli_darwin_amd64"
gsutil cp sha "${output_path}/cli_darwin_amd64.sha256"

sha256sum ${linux_binary} | awk '{print $1}' > sha
gsutil -h 'Content-Disposition:filename=px' cp ${linux_binary} "${output_path}/cli_linux_amd64"
gsutil cp sha "${output_path}/cli_linux_amd64.sha256"
