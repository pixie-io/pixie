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

namespace="plc"

export LANG=C
export LC_ALL=C

kubectl create secret generic -n "${namespace}" \
  cloud-auth-secrets \
  --from-literal=jwt-signing-key="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 64 | head -n 1)"

kubectl create secret generic -n "${namespace}" \
  pl-hydra-secrets \
  --from-literal=SECRETS_SYSTEM="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 64 | head -n 1)" \
  --from-literal=OIDC_SUBJECT_IDENTIFIERS_PAIRWISE_SALT="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 64 | head -n 1)" \
  --from-literal=CLIENT_SECRET="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 64 | head -n 1)"

kubectl create secret generic -n "${namespace}" \
  pl-db-secrets \
  --from-literal=PL_POSTGRES_USERNAME="pl" \
  --from-literal=PL_POSTGRES_PASSWORD="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 24 | head -n 1)" \
  --from-literal=database-key="$(< /dev/urandom tr -dc 'a-zA-Z0-9#$%&().' | fold -w 24 | head -n 1)"

kubectl create secret generic -n "${namespace}" \
  cloud-session-secrets \
  --from-literal=session-key="$(< /dev/urandom tr -dc 'a-zA-Z0-9' | fold -w 24 | head -n 1)"

SERVICE_TLS_CERTS="$(mktemp -d)"
pushd "${SERVICE_TLS_CERTS}" || exit 1

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
DNS.3   = *.pl-nats.${namespace}.svc
DNS.4   = *.pl-nats
DNS.5   = pl-nats
DNS.6   = *.local
DNS.7   = localhost
DNS.8   = kratos
EOS

openssl genrsa -out ca.key 4096
openssl req -new -x509 -sha256 -days 365 -key ca.key -out ca.crt -subj "/O=Pixie/CN=pixie.local"

openssl genrsa -out server.key 4096
openssl req -new -sha256 -key server.key -out server.csr -config ssl.conf -subj "/O=Pixie/CN=pixie.server"

openssl x509 -req -sha256 -days 365 -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 \
    -out server.crt -extensions req_ext -extfile ssl.conf

openssl genrsa -out client.key 4096
openssl req -new -sha256 -key client.key -out client.csr -config ssl.conf -subj "/O=Pixie/CN=pixie.client"

openssl x509 -req -sha256 -days 365 -in client.csr -CA ca.crt -CAkey ca.key -set_serial 01 \
    -out client.crt -extensions req_ext -extfile ssl.conf

kubectl create secret generic -n "${namespace}" \
  service-tls-certs \
  --from-file=ca.crt=./ca.crt \
  --from-file=client.crt=./client.crt \
  --from-file=client.key=./client.key \
  --from-file=server.crt=./server.crt \
  --from-file=server.key=./server.key

popd || exit 1

PROXY_TLS_CERTS="$(mktemp -d)"
PROXY_CERT_FILE="${PROXY_TLS_CERTS}/server.crt"
PROXY_KEY_FILE="${PROXY_TLS_CERTS}/server.key"

mkcert \
  -cert-file "${PROXY_CERT_FILE}" \
  -key-file "${PROXY_KEY_FILE}" \
  dev.withpixie.dev "*.dev.withpixie.dev" localhost 127.0.0.1 ::1

kubectl create secret tls -n "${namespace}" \
  cloud-proxy-tls-certs \
  --cert="${PROXY_CERT_FILE}" \
  --key="${PROXY_KEY_FILE}"
