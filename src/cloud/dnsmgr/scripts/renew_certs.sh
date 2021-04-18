#!/usr/bin/env bash

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set -e
workspace=$(bazel info workspace 2> /dev/null)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
LEGO=${workspace}/lego

if [ $# -ne 2 ]; then
  echo "Usage: $0 <email> <original_certs_dir>"
  exit 1
fi
EMAIL=$1
ORIGINAL_CERTS_DIR=$2
NEW_CERTS_DIR=/tmp/lego_certs

renew_certs() {
  if [ $# -ne 1 ]; then
    echo "Expected 1 Argument: DOMAIN. Received $#."
    exit 1
  fi

  DOMAIN="$1"


  python3 "${SCRIPT_DIR}/renew_certs_for_domain.py" \
      "${DOMAIN}" \
      "${ORIGINAL_CERTS_DIR}" \
      "${NEW_CERTS_DIR}" \
      "${EMAIL}" \
      --lego "${LEGO}" \
      --renew_all
}

if [ -d "${NEW_CERTS_DIR}" ]; then
    echo "${NEW_CERTS_DIR} already exists. Please delete it."
    exit 1
fi

BACKUP_DIR="${ORIGINAL_CERTS_DIR}.backup/"

echo "backing up ${ORIGINAL_CERTS_DIR} to ${BACKUP_DIR}"
cp -r "${ORIGINAL_CERTS_DIR}" "${BACKUP_DIR}"
cp -r "${ORIGINAL_CERTS_DIR}" "${NEW_CERTS_DIR}"

# Save the original gcp project to return to the original state after running this script.
ORIGINAL_GCP_PROJECT=$(gcloud config get-value project)

echo "Renewing the production certificates."

gcloud config set project pixie-prod
export GCE_PROJECT="pixie-prod"
renew_certs "clusters.withpixie.ai"
renew_certs "clusters.staging.withpixie.dev"

# Prepare the output file.
echo "Renewing the dev, testing, and nightly certificates"

gcloud config set project pl-dev-infra
export GCE_PROJECT="pl-dev-infra"
renew_certs "clusters.dev.withpixie.dev"
renew_certs "clusters.testing.withpixie.dev"

# Return to the original GCP project.
gcloud config set project "${ORIGINAL_GCP_PROJECT}"
cd "${workspace}/credentials"
# If you don't rm first then the files will be appended to rather than replaced.
rm -rf "${workspace}/credentials/certs"
mkdir "${workspace}/credentials/certs"
"${SCRIPT_DIR}/convert_certs_to_yaml.sh" "${NEW_CERTS_DIR}/certificates" "${workspace}/credentials/certs"

rm -rf "${ORIGINAL_CERTS_DIR}"
mv "${NEW_CERTS_DIR}" "${ORIGINAL_CERTS_DIR}"
echo "Removing backup at ${BACKUP_DIR}"
rm -rf "${BACKUP_DIR}"
