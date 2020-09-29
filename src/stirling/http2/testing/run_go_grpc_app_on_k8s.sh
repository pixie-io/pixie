#!/bin/bash

bazel run src/stirling/http2/testing/go_grpc_client:push_image
bazel run src/stirling/http2/testing/go_grpc_server:push_image

namespace_name="pl-grpc-test"

kubectl create namespace "${namespace_name}"

kubectl --namespace "${namespace_name}" create secret docker-registry image-pull-secret \
  --docker-server=https://gcr.io \
  --docker-username=_json_key \
  --docker-email="${USER}@pixielabs.ai" \
  --docker-password="$(sops -d credentials/k8s/dev/image-pull-secrets.encrypted.json)"

sed "s/{{USER}}/${USER}/" src/stirling/http2/testing/go_grpc_server/deployment.yaml | \
  kubectl apply -f -
sed "s/{{USER}}/${USER}/" src/stirling/http2/testing/go_grpc_client/deployment.yaml | \
  kubectl apply -f -
