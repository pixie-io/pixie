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
WORKSPACE=$(bazel info workspace 2> /dev/null)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

CERTS_DIR="$HOME/lego_certs"
mkdir -p "${CERTS_DIR}/accounts/acme-v02.api.letsencrypt.org/prod@pixielabs.ai/keys/"

sops -d "${WORKSPACE}/credentials/certs/accounts/acme-v02.api.letsencrypt.org/prod@pixielabs.ai/account.json" > "${CERTS_DIR}/accounts/acme-v02.api.letsencrypt.org/prod@pixielabs.ai/account.json"
sops -d "${WORKSPACE}/credentials/certs/accounts/acme-v02.api.letsencrypt.org/prod@pixielabs.ai/keys/prod@pixielabs.ai.key" > "${CERTS_DIR}/accounts/acme-v02.api.letsencrypt.org/prod@pixielabs.ai/keys/prod@pixielabs.ai.key"

function run_lego() {
  lego --email=prod@pixielabs.ai \
       --dns=gcloud \
       --path="${CERTS_DIR}" \
       --key-type rsa4096 \
       --dns.resolvers=8.8.8.8:53 \
       --dns.resolvers=1.1.1.1:53 \
       --dns.resolvers=127.0.0.53:53 \
       --accept-tos \
       --domains="$1" \
       run
}

mapfile -t STAGING_DOMAINS < <(yq e 'keys' credentials/certs/staging/certs.yaml -M | cut -d '-' -f2 | cut -d '_' -f2 | grep -v sops)
mapfile -t PROD_DOMAINS < <(yq e 'keys' credentials/certs/prod/certs.yaml -M | cut -d '-' -f2 | cut -d '_' -f2 | grep -v sops)

typeset -A EXISTING_DOMAINS
for DOMAIN in "${STAGING_DOMAINS[@]}"; do
  EXISTING_DOMAINS["${DOMAIN}"]="true"
done
for DOMAIN in "${PROD_DOMAINS[@]}"; do
  EXISTING_DOMAINS["${DOMAIN}"]="true"
done

function create_uuid_certs() {
  SUBDOMAIN=$1
  for i in {1..10}; do
    echo "Creating $i for $SUBDOMAIN"
    ID=$(uuidgen | cut -c -8)
    while [[ ${EXISTING_DOMAINS[".${ID}.${SUBDOMAIN}"]} == "true" ]]; do
      ID=$(uuidgen | cut -c -8)
    done
    EXISTING_DOMAINS[".${ID}.${SUBDOMAIN}"]="true"
    GCE_PROJECT="pixie-prod" run_lego "*.${ID}.${SUBDOMAIN}"
  done
}

create_uuid_certs "clusters.staging.withpixie.dev"
create_uuid_certs "clusters.withpixie.ai"

"${SCRIPT_DIR}/convert_certs_to_yaml.sh" "${CERTS_DIR}/certificates" "${WORKSPACE}/credentials/certs"
