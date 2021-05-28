#!/bin/bash -e

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

# This script creates a K8s cluster for various environments.

##################
# Arguments
##################

set_default_values() {
  CLUSTER_NAME="dev-cluster-${USER}"
  AUTOSCALING=true
  NUM_NODES=2
  MIN_NODES=1
  MAX_NODES=5
  MACHINE_TYPE=e2-standard-4
  IMAGE_NAME=UBUNTU
  DISK_SIZE=100
}

# Configuration for personal Pixie dev clusters.
set_pixies_cluster_config() {
  PROJECT=pl-pixies
  ZONE=us-west1-a
  NETWORK=projects/pl-pixies/global/networks/dev
  SUBNETWORK=projects/pl-pixies/regions/us-west1/subnetworks/us-west1-0
}

set_stirling_cluster_config() {
  PROJECT=pl-pixies
  ZONE=us-west1-a
  NETWORK=projects/pl-pixies/global/networks/dev
  SUBNETWORK=projects/pl-pixies/regions/us-west1/subnetworks/us-west1-1
}

# Configuration for devinfra clusters -- jenkins, bazel remote builds,
# bazel caching etc.
set_devinfra_cluster_config() {
  PROJECT=pl-dev-infra
  ZONE=us-west1-a
  NETWORK=projects/pl-dev-infra/global/networks/dev
  SUBNETWORK=projects/pl-dev-infra/regions/us-west1/subnetworks/us-west1-0
}

# Configuration for Pixie cloud (prod) cluster.
set_prod_cluster_config() {
  PROJECT=pixie-prod
  ZONE=us-west1-a
  NETWORK=projects/pixie-prod/global/networks/prod
  SUBNETWORK=projects/pixie-prod/regions/us-west1/subnetworks/us-west-1-0
}

set_skylab_cluster_conig() {
  PROJECT=pixie-skylab
  ZONE=us-west1-a
  NETWORK=projects/pixie-skylab/global/networks/default
  SUBNETWORK=projects/pixie-skylab/regions/us-west1/subnetworks/default
}

print_config() {
  echo "Config: "
  echo "  PROJECT          : ${PROJECT}"
  echo "  CLUSTER_NAME     : ${CLUSTER_NAME}"
  echo "  NUM_NODES        : ${NUM_NODES}"
  echo "  MACHINE_TYPE     : ${MACHINE_TYPE}"
  echo "  IMAGE_NAME       : ${IMAGE_NAME}"
  echo "  DISK_SIZE        : ${DISK_SIZE}"
  echo "  ZONE             : ${ZONE}"
  echo "  NETWORK          : ${NETWORK}"
  echo "  SUBNETWORK       : ${SUBNETWORK}"
  echo ""
}

usage() {
  # Reset to default values, so we can print them.
  set_default_values

  echo "Usage: $0 [-c <cluster_name>] [-p|-b|-s] [-m <machine_type>] [-n <num_nodes>] [-i <image>]"
  echo " -p          : Prod cluster config."
  echo " -b          : DevInfra cluster config."
  echo " -s          : Skylab cluster config."
  echo " -S          : Stirling cluster config."
  echo " -f          : Disable autoscaling of the node pool."
  echo " -c <string> : name of your cluster. [default: ${CLUSTER_NAME}]"
  echo " -n <int>    : number of nodes in the cluster [default: ${NUM_NODES}]"
  echo " -m <string> : machine type [default: ${MACHINE_TYPE}]"
  echo " -i <string> : base image [default: ${IMAGE_NAME}] (can also use COS)"
  echo " -d <int>    : disk size per node (GB) [default: ${DISK_SIZE}]"
  echo " -z <string> : zone [default: ${ZONE}]"
  echo "Example: $0 -c dev-cluster-000 -n 4 -i UBUNTU"
  exit
}

parse_args() {
  set_default_values

  # Default is to create a cluster for pixie developers.
  # This can be overridden by the -p/-b/-s flags.
  set_pixies_cluster_config

  local OPTIND
  # Process the command line arguments.
  while getopts "pbsSfc:n:m:i:d:z:" opt; do
    case ${opt} in
      f)
        AUTOSCALING=false
        unset MIN_NODES
        unset MAX_NODES
        ;;
      c)
        CLUSTER_NAME=$OPTARG
        ;;
      n)
        NUM_NODES=$OPTARG
        ;;
      m)
        MACHINE_TYPE=$OPTARG
        ;;
      i)
        IMAGE_NAME=$OPTARG
        ;;
      d)
        DISK_SIZE=$OPTARG
        ;;
      z)
        ZONE=$OPTARG
        ;;
      p)
        set_prod_cluster_config
        ;;
      b)
        set_devinfra_cluster_config
        ;;
      s)
        set_skylab_cluster_config
        ;;
      S)
        set_stirling_cluster_config
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

parse_args "$@"
print_config

AUTOSCALING_ARGS=()
if [[ $AUTOSCALING == true ]]; then
  AUTOSCALING_ARGS=(--enable-autoscaling --min-nodes "${MIN_NODES}" --max-nodes "${MAX_NODES}")
fi

##################
# Start the cluster
##################

gcloud beta container --project "${PROJECT}" clusters create "${CLUSTER_NAME}" \
 --zone "${ZONE}" \
 --no-enable-basic-auth \
 --machine-type "${MACHINE_TYPE}" \
 --image-type ${IMAGE_NAME} \
 --disk-type "pd-ssd" \
 --disk-size ${DISK_SIZE} \
 --cluster-ipv4-cidr=/21 \
 --services-ipv4-cidr=/20 \
 --scopes "https://www.googleapis.com/auth/compute",\
"https://www.googleapis.com/auth/devstorage.read_only",\
"https://www.googleapis.com/auth/logging.write",\
"https://www.googleapis.com/auth/monitoring.write",\
"https://www.googleapis.com/auth/service.management",\
"https://www.googleapis.com/auth/servicecontrol" \
 --num-nodes ${NUM_NODES} \
 "${AUTOSCALING_ARGS[@]}" \
 --enable-ip-alias \
 --network "${NETWORK}" \
 --subnetwork "${SUBNETWORK}" \
 --addons HorizontalPodAutoscaling,HttpLoadBalancing \
 --no-enable-autoupgrade \
 --no-enable-autorepair \
 --labels k8s-dev-cluster=\
 --security-group="gke-security-groups@pixielabs.ai" \
 --no-enable-stackdriver-kubernetes

if [ $? -ne 0 ]; then
  exit
else
  echo "Cluster created."
  echo "To delete the cluster run:"
  echo "  gcloud beta container --project ${PROJECT} clusters delete ${CLUSTER_NAME} --zone ${ZONE}"
fi
