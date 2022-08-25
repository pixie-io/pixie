#!/bin/bash
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

gcloud_project="${gcloud_project:?}"
gcloud_region="${gcloud_region:?}"
gcloud_network="${gcloud_network:?}"
namespace="${namespace:?}"
resource_name="${resource_name:?}"
resource_name_underscores="${resource_name_underscores:?}"
workspace="${workspace:?}"
credentials_path="${credentials_path:?}"
k8s_path="${k8s_path:?}"

encrypt() {
  file="$1"

  pushd "${credentials_path}" > /dev/null || true

  sops --encrypt -i "${file}"

  popd > /dev/null || true
}

create_service_tls_certs() {
  echo "Creating service-tls-certs secret"
  TMPDIR="$(mktemp -d)"
  pushd "$TMPDIR" > /dev/null || true

  cat << EOS >> ssl.conf
[ req ]
default_bits       = 4096
distinguished_name = req_distinguished_name
req_extensions     = req_ext

[ req_distinguished_name ]

[ req_ext ]
subjectAltName = @alt_names

[alt_names]
DNS.1   = *.${namespace}
DNS.2   = *.${namespace}.svc.cluster.local
DNS.3   = *.local
DNS.4   = localhost
EOS

  openssl genrsa -out ca.key 4096
  openssl req -new -x509 -sha256 -days 365 -key ca.key -out ca.crt -subj "/O=Pixie/CN=pixie.local"

  openssl genrsa -out server.key 4096
  openssl req -new -sha256 -key server.key -out server.csr -config ssl.conf -subj "/O=Pixie/CN=pixie.local"

  openssl x509 -req -sha256 -days 365 -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 \
      -out server.crt -extensions req_ext -extfile ssl.conf

  openssl genrsa -out client.key 4096
  openssl req -new -sha256 -key client.key -out client.csr -config ssl.conf -subj "/O=Pixie/CN=pixie.local"

  openssl x509 -req -sha256 -days 365 -in client.csr -CA ca.crt -CAkey ca.key -set_serial 01 \
      -out client.crt -extensions req_ext -extfile ssl.conf

  file="${credentials_path}/service_tls_certs.yaml"
  kubectl create secret generic -n "${namespace}" \
    --dry-run="client" \
    -o yaml \
    service-tls-certs \
    --from-file=ca.crt=./ca.crt \
    --from-file=client.crt=./client.crt \
    --from-file=client.key=./client.key \
    --from-file=server.crt=./server.crt \
    --from-file=server.key=./server.key > "${file}"

  encrypt "${file}"


  popd > /dev/null || true
  rm -rf "$TMPDIR"
}

create_postgres_instance() {
  instance_name="${resource_name}"
  instance_id="${gcloud_project}:${gcloud_region}:${instance_name}"
  username="${resource_name_underscores}"
  password="$(openssl rand --base64 48)"
  database_name="${resource_name_underscores}"
  service_account_name="${resource_name}-db-access"
  service_account_email="${service_account_name}@${gcloud_project}.iam.gserviceaccount.com"
  sa_key_path="$(mktemp)"
  secrets_name="${resource_name}-db-secrets"


  echo "Creating postgres instance"

  gcloud sql instances create \
    --project="${gcloud_project}" \
    "${instance_name}" \
    --database-version=POSTGRES_14 \
    --cpu=2 \
    --memory=8GB \
    --region="${gcloud_region}" \
    --network="projects/${gcloud_project}/global/networks/${gcloud_network}" \
    --no-assign-ip \
    --availability-type="zonal"

  gcloud sql users create \
    --project="${gcloud_project}" \
    "${username}" \
    --instance="${instance_name}" \
    --password="${password}" \
    --type=BUILT_IN

  gcloud sql databases create \
    "${database_name}" \
    --project="${gcloud_project}" \
    --instance="${instance_name}"

  gcloud iam service-accounts create \
    --project="${gcloud_project}" \
    "${service_account_name}" \
    --display-name="${service_account_name}"

  gcloud iam service-accounts keys create \
    --project="${gcloud_project}" \
    "${sa_key_path}" \
    --iam-account="${service_account_email}"

  gcloud projects add-iam-policy-binding \
    "${gcloud_project}" \
    --member="serviceAccount:${service_account_email}" \
    --role="roles/cloudsql.client" \
    --quiet \
    --condition="title=${resource_name_underscores}_instance_cond,expression=resource.name == \"projects/${gcloud_project}/instances/${instance_name}\""

  cat << EOF > "${k8s_path}/db_config.yaml"
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: px-perf-db-config
data:
  # This is localhost because we proxy postgres through a sidecar.
  PL_POSTGRES_HOSTNAME: localhost
  PL_POSTGRES_PORT: "5432"
  PL_POSTGRES_DB: ${database_name}
  PL_POSTGRES_INSTANCE: ${instance_id}
EOF

  secret_path="${credentials_path}/db_secrets.yaml"
  kubectl create secret generic \
    -n "${namespace}" \
    --dry-run="client" \
    --output="yaml" \
    "${secrets_name}" \
    --from-literal="PL_POSTGRES_USERNAME=${username}" \
    --from-literal="PL_POSTGRES_PASSWORD=${password}" \
    --from-file="db_service_account.json=${sa_key_path}" \
    > "${secret_path}"

  encrypt "${secret_path}"

  rm "${sa_key_path}"
}

create_bq_dataset_service_account() {
  echo "Creating service account for bigquery"

  service_account_name="${resource_name}-bq-access"
  service_account_email="${service_account_name}@${gcloud_project}.iam.gserviceaccount.com"
  sa_key_path="$(mktemp)"
  dataset_name="${resource_name_underscores}"
  secrets_name="${resource_name}-bq-secrets"

  gcloud iam service-accounts create \
    --project="${gcloud_project}" \
    "${service_account_name}" \
    --display-name="${service_account_name}"

  gcloud iam service-accounts keys create \
    --project="${gcloud_project}" \
    "${sa_key_path}" \
    --iam-account="${service_account_email}"

  echo "Creating bigquery dataset"

  bq --location="${gcloud_region}" mk \
    --dataset \
    --description="Dataset to store perf experiment results" \
    "${gcloud_project}:${dataset_name}"

  # Add service account as an OWNER for the dataset.
  tmppath="$(mktemp)"
  bq --location="${gcloud_region}" show \
    --format=json \
    "${gcloud_project}:${dataset_name}" \
    | jq '.access += [{"role": "OWNER","userByEmail": "'"${service_account_email}"'"}]' \
    > "${tmppath}"

  bq --location="${gcloud_region}" update \
    --source "${tmppath}" \
    "${gcloud_project}:${dataset_name}"

  rm "${tmppath}"

  secret_path="${credentials_path}/bq_secrets.yaml"
  kubectl create secret generic \
    -n "${namespace}" \
    --dry-run="client" \
    --output="yaml" \
    "${secrets_name}" \
    --from-file="bq.client.default.credentials_file=${sa_key_path}" \
    > "${secret_path}"

  encrypt "${secret_path}"

  rm "${sa_key_path}"

  cat << EOF > "${k8s_path}/bq_config.yaml"
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: px-perf-bq-config
data:
  PL_BQ_PROJECT: ${gcloud_project}
  PL_BQ_DATASET: ${dataset_name}
  PL_BQ_SA_KEY_PATH: "/creds/bq.client.default.credentials_file"
  PL_BQ_RESULTS_TABLE: "results"
  PL_BQ_SPECS_TABLE: "specs"
EOF
}
