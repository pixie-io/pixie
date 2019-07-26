#!/usr/bin/env bash

namespace=pl
workspace=$(bazel info workspace 2> /dev/null)

create_namespace() {
    kubectl get namespaces ${namespace} 2> /dev/null
    if [ $? -ne 0 ]; then
      kubectl create namespace ${namespace}
    fi
}

create_namespace
${workspace}/scripts/load_secrets.sh ${namespace}
${workspace}/scripts/deploy_cluster_operators.sh ${namespace}

