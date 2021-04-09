#!/usr/bin/env bash
set -e

if [ "$#" -ne 2 ]; then
  echo "This script requires exactly two argument: <namespace> <secret type : dev, prod, etc.>"
fi
namespace=$1
secret_type=$2

workspace=$(bazel info workspace 2> /dev/null)
credentials_path=${workspace}/credentials/k8s/${secret_type}

if [ ! -d "${credentials_path}" ]; then
  echo "Credentials path \"${credentials_path}\" does not exist. Did you slect the right secret type?"
  exit 1
fi

shopt -s nullglob
# Apply configs.
for yaml in "${credentials_path}"/configs/*.yaml; do
  echo "Loading: ${yaml}"
  sops --decrypt "${yaml}" | kubectl apply -n "${namespace}" -f -
done
# Apply secrets.
for yaml in "${credentials_path}"/*.yaml; do
  echo "Loading: ${yaml}"
  sops --decrypt "${yaml}" | kubectl apply -n "${namespace}" -f -
done
