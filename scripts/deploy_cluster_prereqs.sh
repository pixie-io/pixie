#!/usr/bin/env bash

workspace=$(bazel info workspace 2> /dev/null)

source ${workspace}/scripts/script_utils.sh

#############################
# Deploy NATS and etcd
#############################

namespace=pl

kubectl get namespaces ${namespace} 2> /dev/null
if [ $? -ne 0 ]; then
  kubectl create namespace ${namespace}
fi

nats_deploy() {
  kubectl apply --namespace=${namespace} -f ${workspace}/src/services/nats
}

etcd_deploy() {
  kubectl apply --namespace=${namespace} -f ${workspace}/src/services/etcd
}

# Load nats and etcd, we need to run our services.
# These commands might fail waiting for the operator to come up, so we
# retry them a few times.
retry nats_deploy 5 30
retry etcd_deploy 5 30
