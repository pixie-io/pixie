#!/bin/bash

##################
# Arguments
##################

set_default_values() {
  NUM_NODES=4
  MACHINE_TYPE=n1-standard-4
  IMAGE_NAME=UBUNTU
  DISK_SIZE=100
  BARE_CLUSTER=false
}

print_config() {
  echo "Config: "
  echo "  CLUSTER_NAME     : ${CLUSTER_NAME}"
  echo "  NUM_NODES        : ${NUM_NODES}"
  echo "  MACHINE_TYPE     : ${MACHINE_TYPE}"
  echo "  IMAGE_NAME       : ${IMAGE_NAME}"
  echo "  DISK_SIZE        : ${DISK_SIZE}"
  echo "  BARE_CLUSTER     : ${BARE_CLUSTER}"
  echo ""
}

usage() {
  # Reset to default values, so we can print them.
  set_default_values

  echo "Usage: $0 <cluster_name> [-b] [-m <machine_type>] [-n <num_nodes>] [-i <image>]"
  echo " -b          : bare cluster (do not deploy any services)"
  echo " -n <int>    : number of nodes in the cluster [default: ${NUM_NODES}]"
  echo " -m string>> : machine type [default: ${MACHINE_TYPE}]"
  echo " -i <string> : base image [default: ${IMAGE_NAME}]"
  echo " -d <int>    : disk size per node (GB) [default: ${DISK_SIZE}]"
  echo "Example: $0 dev-cluster-000 -n 4 -i UBUNTU"
  exit
}

parse_args() {
  if [ $# -lt 1 ]; then
    usage
  fi

  # Positional arguments (required).
  CLUSTER_NAME=$1
  shift

  local OPTIND
  # Process the command line arguments.
  while getopts "bn:m:i:" opt; do
    case ${opt} in
      b)
        BARE_CLUSTER=true
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
      i)
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

PIXIE_ROOT_DIR=$(bazel info workspace 2> /dev/null)


##################
# Start the cluster
##################

gcloud beta container --project "pl-dev-infra" clusters create ${CLUSTER_NAME} \
 --zone "us-west1-a" \
 --username "admin" \
 --cluster-version "1.11.8-gke.6" \
 --machine-type "${MACHINE_TYPE}" \
 --image-type ${IMAGE_NAME} \
 --disk-type "pd-standard" \
 --disk-size ${DISK_SIZE} \
 --scopes "https://www.googleapis.com/auth/compute",\
"https://www.googleapis.com/auth/devstorage.read_only",\
"https://www.googleapis.com/auth/logging.write",\
"https://www.googleapis.com/auth/monitoring.write",\
"https://www.googleapis.com/auth/service.management",\
"https://www.googleapis.com/auth/servicecontrol" \
 --num-nodes ${NUM_NODES} \
 --enable-cloud-logging \
 --enable-cloud-monitoring \
 --no-enable-ip-alias \
 --network "projects/pl-dev-infra/global/networks/dev" \
 --subnetwork "projects/pl-dev-infra/regions/us-west1/subnetworks/us-west1-0" \
 --addons HorizontalPodAutoscaling,KubernetesDashboard \
 --no-enable-autoupgrade \
 --no-enable-autorepair \
 --labels k8s-dev-cluster=

if [ $? -ne 0 ]; then
  exit
else
  echo "Cluster created."
  echo "To delete the cluster run:"
  echo "  gcloud beta container --project "pl-dev-infra" clusters delete ${CLUSTER_NAME}"
fi

##################
# Deploy standard services
##################

if [ ! ${BARE_CLUSTER} = true ]; then
  # Sockshop
  kubectl apply -f $PIXIE_ROOT_DIR/demos/applications/sockshop/kubernetes_manifests/sock-shop-ns.yaml
  sleep 5
  kubectl apply -f $PIXIE_ROOT_DIR/src/pixielabs.ai/pixielabs/demos/applications/sockshop/kubernetes_manifests
  kubectl apply -f $PIXIE_ROOT_DIR/src/pixielabs.ai/pixielabs/demos/applications/sockshop/load_generation

  # TODO(oazizi/philkuz): Enable monitoring through this script.
  # $PIXIE_ROOT_DIR/demos/applications/sockshop/monitoring_manifests/create_monitoring.sh
fi
