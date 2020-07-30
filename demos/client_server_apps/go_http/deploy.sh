#!/usr/bin/env bash

repo_path=$(bazel info workspace)

bazel run -c opt //demos/client_server_apps/go_http/server:push_simple_http_server_image
kubectl apply -f "${repo_path}/demos/client_server_apps/go_http/k8s.yaml"
