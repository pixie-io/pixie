#!/bin/bash

# This script is use to create a K8s cluster in either the dev/prod environments.

##################
# Arguments
##################

set_default_values() {
  NUM_NODES=2
  MACHINE_TYPE=n1-standard-1
  IMAGE_NAME=UBUNTU
  DISK_SIZE=100
  ZONE=us-west1-a

  PROD_MODE=false
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

print_config() {
  echo "Config: "
  echo "  PROD_MODE        : ${PROD_MODE}"
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

  echo "Usage: $0 [-c <cluster_name>] [-p] [-b] [-m <machine_type>] [-n <num_nodes>] [-i <image>]"
  echo " -c <string> : name of your cluster. [default: dev-cluster-${USER}]"
  echo " -p          : Prod cluster config, must appear as first argument. [default: ${PROD_MODE}]"
  echo " -n <int>    : number of nodes in the cluster [default: ${NUM_NODES}]"
  echo " -m <string> : machine type [default: ${MACHINE_TYPE}]"
  echo " -i <string> : base image [default: ${IMAGE_NAME}] (can also use COS)"
  echo " -d <int>    : disk size per node (GB) [default: ${DISK_SIZE}]"
  echo "Example: $0 -c dev-cluster-000 -n 4 -i UBUNTU"
  exit
}

parse_args() {
  CLUSTER_NAME="dev-cluster-${USER}"

  echo "${CLUSTER_NAME:0:1}"
  # Make sure the cluster name does not start with dash.
  # User is probably trying to pass a flag without the required positional argument.
  if [ "${CLUSTER_NAME:0:1}" = "-" ]; then
    usage
  fi

  # Check to see if prod flag is specified so that we can change the defaults.
  if [ "$1" = "-p" ] ; then
    set_default_prod_values
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
 --scopes "https://www.googleapis.com/auth/compute",\
"https://www.googleapis.com/auth/devstorage.read_only",\
"https://www.googleapis.com/auth/logging.write",\
"https://www.googleapis.com/auth/monitoring.write",\
"https://www.googleapis.com/auth/service.management",\
"https://www.googleapis.com/auth/servicecontrol" \
 --num-nodes ${NUM_NODES} \
 --enable-ip-alias \
 --enable-cloud-logging \
 --enable-cloud-monitoring \
 --network "${NETWORK}" \
 --subnetwork "${SUBNETWORK}" \
 --addons HorizontalPodAutoscaling,KubernetesDashboard,HttpLoadBalancing \
 --no-enable-autoupgrade \
 --no-enable-autorepair \
 --labels k8s-dev-cluster=

if [ $? -ne 0 ]; then
  exit
else
  echo "Cluster created."
  echo "To delete the cluster run:"
  echo "  gcloud beta container --project ${PROJECT} clusters delete ${CLUSTER_NAME} --zone ${ZONE}"
fi
