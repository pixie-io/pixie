#!/usr/bin/env bash

set -ex

printenv

repo_path=$(pwd)
release_tag=${TAG_NAME##*/v}
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

echo "The release tag is: ${release_tag}"

bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name vizier --versions_file "${versions_file}"

bazel run --stamp -c opt --define BUNDLE_VERSION="${release_tag}" --stamp //k8s/vizier:vizier_images_push
bazel build --stamp -c opt --define BUNDLE_VERSION="${release_tag}" --stamp //k8s/vizier:vizier_yamls

output_path="gs://pixie-prod-artifacts/vizier/${release_tag}"
yamls_tar="${repo_path}/bazel-bin/k8s/vizier/vizier_yamls.tar"

sha256sum "${yamls_tar}" > sha
gsutil cp "${yamls_tar}" "${output_path}/yamls.tar"
gsutil cp sha "${output_path}/yamls.tar.sha256"
