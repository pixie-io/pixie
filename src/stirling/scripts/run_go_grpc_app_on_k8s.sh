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

bazel run --config=stamp src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client:push_image
bazel run --config=stamp src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server:push_image

namespace_name="px-go-grpc"

kubectl create namespace "${namespace_name}"

kubectl --namespace "${namespace_name}" create secret docker-registry image-pull-secret \
  --docker-server=https://gcr.io \
  --docker-username=_json_key \
  --docker-email="${USER}@pixielabs.ai" \
  --docker-password="$(sops -d private/credentials/k8s/dev/image-pull-secrets.encrypted.json)"

sed "s/{{USER}}/${USER}/" src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/deployment.yaml | \
  kubectl apply -f -
sed "s/{{USER}}/${USER}/" src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client/deployment.yaml | \
  kubectl apply -f -
