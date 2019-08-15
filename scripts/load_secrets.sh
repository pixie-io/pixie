#!/usr/bin/env bash

# Assume pl namespace by default.
namespace=pl
if [ "$#" -eq 1 ]; then
    namespace=$1
fi

workspace=$(bazel info workspace 2> /dev/null)

load_certs() {
    cd ${workspace}/src/utils/pl_admin/
    bazel run :pl_deploy -- install-certs
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
