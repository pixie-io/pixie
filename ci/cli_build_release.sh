#!/usr/bin/env bash

set -e

printenv

repo_path=$(pwd)
release_tag=$($TAG_NAME | sed 's|release/cli-\(v.*\)|\1|')
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"

bazel build -c opt --stamp //src/utils/pixie_cli:pixie_mac

bazel build -c opt --stamp //src/utils/pixie_cli:pixie


gsutil cp bazel-bin/src/utils/pixie_cli/darwin_amd64_pure_stripped/pixie_mac \
       "gs://pixie-prod-artifacts/cli/${release_tag}/cli_darwin_amd64"

gsutil cp bazel-bin/src/utils/pixie_cli/linux_amd64_stripped/pixie \
       "gs://pixie-prod-artifacts/cli/${release_tag}/cli_linux_amd64"
