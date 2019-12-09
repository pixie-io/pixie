#!/bin/bash -e

usage() {
  echo "This script runs agent in headless mode on GKE for a specified amount of time."
  echo "It then collects logs and deletes the headless agent pods."
  echo ""
  echo "Usage: $0 <duration in seconds>"
  exit
}

parse_args() {
  if [[ $# -lt 1 ]]; then
    usage
    echo "Needs at least 1 argument for running time, exit ..."
    exit 1
  fi
  T=$1
}

NAMESPACE="pl-${USER}-headless-agent"
YAML="$(kustomize build k8s/vizier/headless_agent | sed 's/{{USER}}/'${USER}'/g')"

parse_args "$@"

echo ""
echo "-------------------------------------------"
echo "Building and pushing agent image"
echo "-------------------------------------------"

bazel run //src/vizier/services/agent:push_test_pem_image

if [[ "$(kubectl get namespace ${NAMESPACE} 2>/dev/null | grep ^${NAMESPACE} | wc -l)" != "0" ]];
then
  echo ""
  echo "-------------------------------------------"
  echo "Delete any old instances"
  echo "-------------------------------------------"

  kubectl delete namespace ${NAMESPACE} &>/dev/null
fi

echo ""
echo "-------------------------------------------"
echo "Deploying headless agent"
echo "-------------------------------------------"

kubectl create namespace ${NAMESPACE}
echo "${YAML}" | kubectl apply -f -

echo ""
echo "-------------------------------------------"
echo "Waiting ${T} seconds to collect data"
echo "-------------------------------------------"

sleep $T

echo ""
echo "-------------------------------------------"
echo "Listing pods"
echo "-------------------------------------------"

kubectl get pods --namespace ${NAMESPACE} | grep ^agent
pods=$(kubectl get pods --namespace ${NAMESPACE} | grep ^agent | grep Running | cut -f1 -d' ')

echo ""
echo "-------------------------------------------"
echo "Collecting logs"
echo "-------------------------------------------"

timestamp=$(date +%s)
for pod in $pods; do
  filename=log$timestamp.$pod
  kubectl logs --namespace ${NAMESPACE} $pod > $filename
  echo $filename
done

echo ""
echo "-------------------------------------------"
echo "Cleaning up (deleting namespace)"
echo "-------------------------------------------"

kubectl delete namespace ${NAMESPACE}
