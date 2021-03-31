#!/bin/bash

scriptdir=$(dirname "$(realpath "$0")")
certdir=$scriptdir/../certs

target="//src/stirling/testing/demo_apps/go_grpc_tls_pl/server:server"

# Note that client assumes this JWT signing key, so do not change it.

bazel run "$target" -- --jwt_signing_key=123456 \
                       --server_tls_cert="$certdir"/server.crt \
                       --server_tls_key="$certdir"/server.key \
                       --tls_ca_cert="$certdir"/rootCA.crt
