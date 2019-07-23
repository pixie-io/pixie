#!/usr/bin/env bash

# Assume pl namespace by default.
namespace=pl
if [ "$#" -eq 1 ]; then
    namespace=$1
fi

workspace=$(bazel info workspace 2> /dev/null)

load_certs() {
    kubectl -n ${namespace} delete secret proxy-tls-certs 2> /dev/null || true
    kubectl -n ${namespace} delete secret service-tls-certs 2> /dev/null || true
    kubectl -n ${namespace} delete secret etcd-peer-tls-certs 2> /dev/null || true
    kubectl -n ${namespace} delete secret etcd-client-tls-certs 2> /dev/null || true
    kubectl -n ${namespace} delete secret etcd-server-tls-certs 2> /dev/null || true

    kubectl -n ${namespace} create secret tls proxy-tls-certs \
        --key ${workspace}/src/services/certs/server.key \
        --cert ${workspace}/src/services/certs/server.crt

    kubectl -n ${namespace} create secret generic service-tls-certs \
        --from-file=server.key=${workspace}/src/services/certs/server.key \
        --from-file=server.crt=${workspace}/src/services/certs/server.crt \
        --from-file=ca.crt=${workspace}/src/services/certs/ca.crt \
        --from-file=client.key=${workspace}/src/services/certs/client.key \
        --from-file=client.crt=${workspace}/src/services/certs/client.crt

    kubectl -n ${namespace} create secret generic etcd-peer-tls-certs \
        --from-file=peer.key=${workspace}/src/services/certs/server.key \
        --from-file=peer.crt=${workspace}/src/services/certs/server.crt \
        --from-file=peer-ca.crt=${workspace}/src/services/certs/ca.crt

    kubectl -n ${namespace} create secret generic etcd-client-tls-certs \
        --from-file=etcd-client.key=${workspace}/src/services/certs/client.key \
        --from-file=etcd-client.crt=${workspace}/src/services/certs/client.crt \
        --from-file=etcd-client-ca.crt=${workspace}/src/services/certs/ca.crt

    kubectl -n ${namespace} create secret generic etcd-server-tls-certs \
        --from-file=server.key=${workspace}/src/services/certs/server.key \
        --from-file=server.crt=${workspace}/src/services/certs/server.crt \
        --from-file=server-ca.crt=${workspace}/src/services/certs/ca.crt
}

#Loads the secrets used by the dev environment.
load_dev_secrets() {
    kubectl -n ${namespace} delete secret pl-app-secrets 2> /dev/null || true
    kubectl -n ${namespace} create secret generic pl-app-secrets \
        --from-literal=jwt-signing-key=ABCDEFG \
        --from-literal=session-key=test-session-key \
        --from-literal=auth0-client-id=qaAfEHQT7mRt6W0gMd9mcQwNANz9kRup \
        --from-literal=auth0-client-secret=_rY9isTWtKgx2saBXNKZmzAf1y9pnKvlm-WdmSVZOFHb9OQtWHEX4Nrh3nWE5NNt
}

load_certs
load_dev_secrets
