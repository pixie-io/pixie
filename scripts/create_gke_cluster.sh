#!/bin/bash

# This script is use to create a K8s cluster in either the dev/prod/skylab environments.

##################
# Arguments
##################

set_default_values() {
  CLUSTER_NAME="dev-cluster-${USER}"
  NUM_NODES=2
  MACHINE_TYPE=e2-standard-4
  IMAGE_NAME=UBUNTU
  DISK_SIZE=100
  ZONE=us-west1-a

  PROD_MODE=false
  SKYLAB_MODE=false
  PROJECT=pl-dev-infra
  NETWORK=projects/pl-dev-infra/global/networks/dev
  SUBNETWORK=projects/pl-dev-infra/regions/us-west1/subnetworks/us-west1-0
}

set_default_prod_values() {
  PROD_MODE=true
  PROJECT=pixie-prod
  NETWORK=projects/pixie-prod/global/networks/prod
  SUBNETWORK=projects/pixie-prod/regions/us-west1/subnetworks/us-west-1-0
}

set_default_skylab_values() {
  SKYLAB_MODE=true
  PROJECT=pixie-skylab
  NETWORK=projects/pixie-skylab/global/networks/default
  SUBNETWORK=projects/pixie-skylab/regions/us-west1/subnetworks/default
}

print_config() {
  echo "Config: "
  echo "  PROD_MODE        : ${PROD_MODE}"
  echo "  SKYLAB_MODE      : ${SKYLAB_MODE}"
  echo "  PROJECT          : ${PROJECT}"
  echo "  CLUSTER_NAME     : ${CLUSTER_NAME}"
  echo "  NUM_NODES        : ${NUM_NODES}"
  echo "  MACHINE_TYPE     : ${MACHINE_TYPE}"
  echo "  IMAGE_NAME       : ${IMAGE_NAME}"
  echo "  DISK_SIZE        : ${DISK_SIZE}"
  echo "  NETWORK          : ${NETWORK}"
  echo "  SUBNETWORK       : ${SUBNETWORK}"
  echo ""
}

usage() {
  # Reset to default values, so we can print them.
  set_default_values

  echo "Usage: $0 [-p] [-c <cluster_name>] [-b] [-m <machine_type>] [-n <num_nodes>] [-i <image>]"
  echo " -p          : Prod cluster config, must appear as first argument. [default: ${PROD_MODE}]"
  echo " -s          : Skylab cluster config, must appear as first argument. [default: ${SKYLAB_MODE}]"
  echo " -c <string> : name of your cluster. [default: ${CLUSTER_NAME}]"
  echo " -n <int>    : number of nodes in the cluster [default: ${NUM_NODES}]"
  echo " -m <string> : machine type [default: ${MACHINE_TYPE}]"
  echo " -i <string> : base image [default: ${IMAGE_NAME}] (can also use COS)"
  echo " -d <int>    : disk size per node (GB) [default: ${DISK_SIZE}]"
  echo "Example: $0 -c dev-cluster-000 -n 4 -i UBUNTU"
  exit
}

parse_args() {
  # Check to see if prod flag is specified so that we can change the defaults.
  if [ "$1" = "-p" ] ; then
    set_default_prod_values
    shift
  fi

  # Check to see if skylaab  flag is specified so that we can change the defaults.
  if [ "$1" = "-s" ] ; then
    set_default_skylab_values
    shift
  fi

  local OPTIND
  # Process the command line arguments.
  while getopts "c:n:m:i:d:" opt; do
    case ${opt} in
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

set_default_values
parse_args "$@"
print_config

##################
# Start the cluster
##################

gcloud beta container --project "${PROJECT}" clusters create ${CLUSTER_NAME} \
 --zone "${ZONE}" \
 --username "admin" \
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
