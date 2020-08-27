#!/usr/bin/env bash

set -e

DEFAULT_INSTALL_PATH=/usr/local/bin
ARTIFACT_BASE_PATH="https://storage.googleapis.com/pixie-prod-artifacts/cli"
ARTIFACT_NAME=cli_darwin_amd64
USE_VERSION=${PL_CLI_VERSION:-latest}
INSTALL_PATH=${INSTALL_PATH:-${DEFAULT_INSTALL_PATH}}

# First check if the OS is Linux.
if [[ "$(uname)" = "Linux" ]]; then
    ARTIFACT_NAME=cli_linux_amd64
fi

artifact_url() {
  echo "${ARTIFACT_BASE_PATH}/${USE_VERSION}/${ARTIFACT_NAME}"
}

curl -fsSL "$(artifact_url)" -o /tmp/px_new
sudo mv /tmp/px_new ${INSTALL_PATH}
