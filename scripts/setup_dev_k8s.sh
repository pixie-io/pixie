#!/usr/bin/env bash

vm_driver=kvm2
memory=8192
cpus=8

namespace=pl
workspace=$(bazel info workspace)

# On Linux we need to start minikube if not running.
# On Macos we need to make sure kubernetes docker(docker-desktop) is running.
if [ "$(uname)" == "Darwin" ]; then
  # MacOS
  current_context=$(kubectl config current-context)
  if [ "${current_context}" != "docker-desktop" ]; then
    echo "Make sure kubectl context is docker-desktop"
    exit 1
  fi
  kubectl cluster-info
  if [ $? -ne 0 ]; then
    echo "Kubernetes is down. Please make sure docker-desktop with Kubernetes is running"
    exit 1
  fi
else
  minikube status
  if [ $? -ne 0 ]; then
    minikube start --cpus=${cpus} --memory=${memory} --vm-driver=${vm_driver}
  fi
fi

# Create namespace if it does not exist.
kubectl get namespaces ${namespace} 2> /dev/null
if [ $? -ne 0 ]; then
  kubectl create namespace ${namespace}
fi

${workspace}/scripts/deploy_cluster_prereqs.sh

echo "K8s cluster setup complete!"
