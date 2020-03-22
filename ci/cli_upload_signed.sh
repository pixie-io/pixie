#!/usr/bin/env bash

repo_path=$(bazel info workspace)

# shellcheck source=ci/gcs_utils.sh
. "${repo_path}/ci/gcs_utils.sh"

set -ex

printenv

release_tag=${TAG_NAME##*/v}

output_path="gs://pixie-prod-artifacts/cli/${release_tag}"
copy_artifact_to_gcs "$output_path" "cli_darwin_amd64" "cli_darwin_amd64"
copy_artifact_to_gcs "$output_path" "cli_darwin_amd64.zip" "cli_darwin_amd64.zip"

# Check to see if it's production build. If so we should also write it to the latest directory.
if [[ ! "$release_tag" == *"-"* ]]; then
  output_path="gs://pixie-prod-artifacts/cli/latest"
  copy_artifact_to_gcs "$output_path" "cli_darwin_amd64" "cli_darwin_amd64"
  copy_artifact_to_gcs "$output_path" "cli_darwin_amd64.zip" "cli_darwin_amd64.zip"
fi
