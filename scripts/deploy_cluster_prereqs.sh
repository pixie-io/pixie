#!/usr/bin/env bash

namespace=pl

create_namespace() {
    kubectl get namespaces ${namespace} 2> /dev/null
    if [ $? -ne 0 ]; then
      kubectl create namespace ${namespace}
    fi
}

create_namespace
./load_secrets.sh ${namespace}
./deploy_cluster_operators.sh ${namespace}

