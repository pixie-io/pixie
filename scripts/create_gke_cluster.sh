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
  DYNAMIC_SUBNET=false
  NUM_NODES=2
  MIN_NODES=1
  MAX_NODES=5
  MACHINE_TYPE=e2-standard-4
  IMAGE_NAME=COS_CONTAINERD
  DISK_SIZE=100
  LABELS="k8s-dev-cluster="
}

# Configuration for personal Pixie dev clusters.
set_pixies_cluster_config() {
  PROJECT=pl-pixies
  ZONE=us-west1-a
  NETWORK=projects/pl-pixies/global/networks/dev
  SUBNETWORK_ARGS=(--subnetwork projects/pl-pixies/regions/us-west1/subnetworks/us-west1-0)
}

# Configuration for Pixie cloud (prod) cluster.
set_prod_cluster_config() {
  PROJECT=pixie-prod
  ZONE=us-west1-a
  NETWORK=projects/pixie-prod/global/networks/prod
  SUBNETWORK_ARGS=(--subnetwork projects/pixie-prod/regions/us-west1/subnetworks/us-west-1-0)
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
  echo "  SUBNETWORK       : ${SUBNETWORK_ARGS[1]}"
  echo "  LABELS           : ${LABELS}"
  echo ""
}

usage() {
  # Reset to default values, so we can print them.
  set_default_values

  echo "Usage: $0 [-c <cluster_name>] [-p|-f|-S] [-m <machine_type>] [-n <num_nodes>] [-i <image>] [-d <disk_size>] [-z <zone>]"
  echo " -p          : Prod cluster config."
  echo " -f          : Disable autoscaling of the node pool."
  echo " -S          : Use a dynamically created subnetwork."
  echo " -c <string> : name of your cluster. [default: ${CLUSTER_NAME}]"
  echo " -n <int>    : number of nodes in the cluster [default: ${NUM_NODES}]"
  echo " -m <string> : machine type [default: ${MACHINE_TYPE}]"
  echo " -i <string> : base image [default: ${IMAGE_NAME}]"
  echo " -d <int>    : disk size per node (GB) [default: ${DISK_SIZE}]"
  echo " -z <string> : zone [default: ${ZONE}]"
  echo "Example: $0 -c dev-cluster-000 -n 4 -i UBUNTU"
  exit
}

parse_args() {
  set_default_values

  # Default is to create a cluster for pixie developers.
  # This can be overridden by the -p flags.
  set_pixies_cluster_config

  local OPTIND
  # Process the command line arguments.
  while getopts "pfSc:n:m:i:d:z:" opt; do
    case ${opt} in
      p)
        set_prod_cluster_config
        ;;
      f)
        AUTOSCALING=false
        unset MIN_NODES
        unset MAX_NODES
        ;;
      S)
        DYNAMIC_SUBNET=true
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

if [[ $DYNAMIC_SUBNET == true ]]; then
  SUBNETWORK_ARGS=(--create-subnetwork "name=subnet-${CLUSTER_NAME}")
fi
AUTOSCALING_ARGS=()
if [[ $AUTOSCALING == true ]]; then
  AUTOSCALING_ARGS=(--enable-autoscaling --min-nodes "${MIN_NODES}" --max-nodes "${MAX_NODES}")
fi

print_config

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
 --scopes gke-default,compute-rw \
 --num-nodes ${NUM_NODES} \
 "${AUTOSCALING_ARGS[@]}" \
 --enable-ip-alias \
 --network "${NETWORK}" \
 "${SUBNETWORK_ARGS[@]}" \
 --addons HorizontalPodAutoscaling,HttpLoadBalancing \
 --no-enable-autoupgrade \
 --no-enable-autorepair \
 --labels "${LABELS}" \
 --security-group="gke-security-groups@pixielabs.ai" \
 --logging=NONE \
 --monitoring=NONE

if [ $? -ne 0 ]; then
  exit
else
  echo "Cluster created."
  echo "To delete the cluster run:"
  echo "  gcloud beta container --project ${PROJECT} clusters delete ${CLUSTER_NAME} --zone ${ZONE}"
fi
