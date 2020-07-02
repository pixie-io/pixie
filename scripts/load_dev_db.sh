#!/bin/bash -ex

if [ $# -lt 1 ]; then
  echo "This script requires exactly one argument: <namespace>"
  exit
fi
namespace=$1

repo_path=$(pwd)
versions_file="$(pwd)/src/utils/artifacts/artifact_db_updater/VERSIONS.json"
certs_path=$(pwd)/credentials/certs

# Port-forward the postgres pod.
postgres_pod=$(kubectl get pod --namespace "$namespace" --selector="name=postgres" \
    --output jsonpath='{.items[0].metadata.name}')
kubectl port-forward pods/"$postgres_pod" 5432:5432 -n "$namespace" &

# Update database with Vizier versions.
bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name vizier --versions_file "${versions_file}"
bazel run -c opt //src/utils/artifacts/artifact_db_updater:artifact_db_updater -- \
    --versions_file "${versions_file}" --postgres_db "pl"

# Update database with CLI versions.
bazel run -c opt //src/utils/artifacts/versions_gen:versions_gen -- \
      --repo_path "${repo_path}" --artifact_name cli --versions_file "${versions_file}"
bazel run -c opt //src/utils/artifacts/artifact_db_updater:artifact_db_updater -- \
    --versions_file "${versions_file}" --postgres_db "pl"

git checkout main "$versions_file"

# Update database with SSL certs.
bazel run -c opt //src/cloud/dnsmgr/load_certs:load_certs -- \
    --certs_path "${certs_path}" --postgres_db "pl"

# Kill kubectl port-forward.
kill -15 "$!"
sleep 2
# Make sure process cleans up properly.
kill -9 "$!"
