#!/bin/bash

HAL_DIRECTORY="${HOME}/.hal"
mkdir -p "${HAL_DIRECTORY}"
workspace=$(bazel info workspace 2> /dev/null)

# Copy configs to expected hal directory.
cp -r "${workspace}/dev_infra/spinnaker/hal" "${HAL_DIRECTORY}"

export HAL_KUBE_CONFIG="${HOME}/.kube/config"
if [ -n "$KUBECONFIG" ]
then
export HAL_KUBE_CONFIG=$KUBECONFIG
fi

# Decrypt and copy over secrets files.
cd "${workspace}/credentials/dev_infra" || exit
mkdir -p "${HAL_DIRECTORY}/creds"
export GCS_ACCOUNT_FILE="${HAL_DIRECTORY}/creds/gcs-account.json"
sops -d gcs_account.json > "${GCS_ACCOUNT_FILE}"
export GITHUB_TOKEN_FILE="${HAL_DIRECTORY}/creds/github.txt"
sops -d github_token.txt > "${GITHUB_TOKEN_FILE}"
# shellcheck source=credentials/dev_infra/secrets.sh
source <(sops -d secrets.sh)

envsubst < "${workspace}/dev_infra/spinnaker/hal/config_template" > "${HAL_DIRECTORY}/config"
