#!/usr/bin/env bash

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set -e

vm_driver=kvm2
memory=8192
cpus=8

# Support for minikube profile - for shared machines
has_minikube_profile=false
if [ $# -ne 0 ]; then
  profile=$1
  has_minikube_profile=true
fi

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
  ret=0
  minikube status "${profile_args_str}" || ret=$?
  if [ $ret -ne 0 ]; then
    minikube start --cpus=${cpus} --memory=${memory} --vm-driver=${vm_driver} "${profile_args_str}"
  fi
fi

echo "K8s cluster setup complete!"
