#!/usr/bin/env bash

vm_driver=kvm2
memory=8192
cpus=8

# Support for minikube profile - for shared machines
has_minikube_profile=false
if [ $# -ne 0 ]; then
    profile=$1
    has_minikube_profile=true
fi

workspace=$(bazel info workspace)

profile_args_str=""
if [ "${has_minikube_profile}" = true ]; then
    profile_args_str="-p $profile"
    echo "Minikube Profile: ${profile_args_str}"
fi

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
  minikube status ${profile_args_str}
  if [ $? -ne 0 ]; then
      minikube start --cpus=${cpus} --memory=${memory} --vm-driver=${vm_driver} ${profile_args_str}
  fi
fi

echo "K8s cluster setup complete!"
