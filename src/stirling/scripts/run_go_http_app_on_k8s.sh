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

pixie_root=$(git rev-parse --show-toplevel)
http_app_dir=src/stirling/source_connectors/socket_tracer/protocols/http/testing

bazel run --config=stamp //src/stirling/source_connectors/socket_tracer/protocols/http/testing/go_http_client:push_image
bazel run --config=stamp //src/stirling/source_connectors/socket_tracer/protocols/http/testing/go_http_server:push_image

ns="px-go-http"

# Caller can pass arguments to kubectl via $1 if desired.
# Main use case is to choose a kubeconfig file via `--kubeconfig`.
KUBECTL="kubectl $1"

# Create namespace if it does not exist.
$KUBECTL create namespace "$ns" --dry-run=client -o yaml | $KUBECTL apply -f -

$KUBECTL -n "$ns" create secret docker-registry image-pull-secret \
  --docker-server=https://gcr.io \
  --docker-username=_json_key \
  --docker-email="${USER}@pixielabs.ai" \
  --docker-password="$(sops -d "$pixie_root"/credentials/k8s/dev/image-pull-secrets.encrypted.json)" \
  --dry-run=client --output=yaml | $KUBECTL apply -f -

sed "s/{{USER}}/${USER}/" "$http_app_dir"/go_http_server/deployment.yaml | $KUBECTL apply -n "$ns" -f -
sed "s/{{USER}}/${USER}/" "$http_app_dir"/go_http_client/deployment.yaml | $KUBECTL apply -n "$ns" -f -
