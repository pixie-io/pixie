#!/usr/bin/env bash

# Assume pl namespace by default.
namespace=pl
if [ "$#" -eq 1 ]; then
    namespace=$1
fi

workspace=$(bazel info workspace 2> /dev/null)

source ${workspace}/scripts/script_utils.sh

nats_deploy() {
  kubectl apply -n ${namespace} -k ${workspace}/k8s/vizier_deps/base/nats
}

etcd_deploy() {
  kubectl apply -n ${namespace} -k ${workspace}/k8s/vizier_deps/base/etcd
}

# Load nats and etcd, we need to run our services.
# These commands might fail waiting for the operator to come up, so we
# retry them a few times.
retry nats_deploy 5 30
retry etcd_deploy 5 30
