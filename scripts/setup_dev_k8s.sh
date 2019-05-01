#!/usr/bin/env bash

vm_driver=kvm2
memory=8192
cpus=8

workspace=$(bazel info workspace)
namespace=pl

retry() {
  cmd=$1
  try=${2:-15}       # 15 by default
  sleep_time=${3:-3} # 3 seconds by default

  # Show help if a command to retry is not specified.
  [ -z "$1" ] && echo 'Usage: retry cmd [try=15 sleep_time=3]' && return 1

  # The unsuccessful recursion termination condition (if no retries left)
  [ $try -lt 1 ] && echo 'All retries failed.' && return 1

  # The successful recursion termination condition (if the function succeeded)
  $cmd && return 0

  echo "Execution of '$cmd' failed."

  # Inform that all is not lost if at least one more retry is available.
  # $attempts include current try, so tries left is $attempts-1.
  if [ $((try-1)) -gt 0 ]; then
    echo "There are still $((try-1)) retrie(s) left."
    echo "Waiting for $sleep_time seconds..." && sleep $sleep_time
  fi

  # Recurse
  retry $cmd $((try-1)) $sleep_time
}

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


kubectl get namespaces ${namespace}
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
retry nats_deploy 5 5
retry etcd_deploy 5 5


echo "K8s cluster setup complete!"
