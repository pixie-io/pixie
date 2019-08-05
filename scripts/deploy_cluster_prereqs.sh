#!/usr/bin/env bash

vizier_namespace=pl
cloud_namespace=plc

workspace=$(bazel info workspace 2> /dev/null)

create_namespace() {
    kubectl get namespaces $1 2> /dev/null
    if [ $? -ne 0 ]; then
      kubectl create namespace $1
    fi
}

create_namespace ${vizier_namespace}
create_namespace ${cloud_namespace}

${workspace}/scripts/load_secrets.sh ${vizier_namespace}
${workspace}/scripts/deploy_cluster_operators.sh ${vizier_namespace}
