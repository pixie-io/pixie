#!/bin/bash

scriptdir=$(dirname "$(realpath "$0")")
certdir=$scriptdir/../certs

target="//src/stirling/testing/demo_apps/go_grpc_tls_pl/client:client"

bazel run "$target" -- --client_tls_cert="$certdir"/client.crt \
                       --client_tls_key="$certdir"/client.key \
                       --tls_ca_cert="$certdir"/rootCA.crt
