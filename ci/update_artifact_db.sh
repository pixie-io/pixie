#!/usr/bin/env bash

set -e

versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"

# Print out the versions file so we can inspect.
jq -r . "${versions_file}"

bazel run //src/utils/artifacts/artifact_db_updater:artifact_db_updater_job > manifest.yaml

kubectl apply -f manifest.yaml

kubectl wait --for=condition=complete --timeout=60s job/artifact-db-updater-job

# Remove this after feature is enabled: https://kubernetes.io/docs/concepts/workloads/\
# controllers/jobs-run-to-completion/#clean-up-finished-jobs-automatically
kubectl delete job artifact-db-updater-job
