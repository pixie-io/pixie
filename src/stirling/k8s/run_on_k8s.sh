#!/bin/bash -e

usage() {
  echo "This script runs stirling_wrapper on a K8s cluster for a specified amount of time."
  echo "It then collects and opens the logs, and deletes the stirling_wrapper pods."
  echo
  echo "Usage: $0 [<duration>] [--open_logs]"
  exit 1
}

parse_args() {
  # Set default values.
  OPEN_LOGS=false
  T=30 # Time spent running stirling on the cluster.

  # TODO(yzhao): Might need to switch to getopt() if more flags are added.
  while [ $# -gt 0 ]; do
  case $1 in
      --open_logs)
      OPEN_LOGS=true
      shift
      ;;
      *)
      re='^[0-9]+$'
      if ! [[ $1 =~ $re ]] ; then
        exit 1
      fi
      T=$1
      shift
      ;;
  esac
  done
}

# Script execution starts here

# Always run in the script directory, regardless of where the script is called from.
scriptdir=$(dirname "$0")
cd "$scriptdir"

NAMESPACE=pl-${USER}

parse_args "$@"

echo ""
echo "-------------------------------------------"
echo "Building and pushing stirling_wrapper image"
echo "-------------------------------------------"

bazel build //src/stirling/binaries:stirling_wrapper_image
bazel run //src/stirling/binaries:push_stirling_wrapper_image

echo ""
echo "-------------------------------------------"
echo "Delete any old instances"
echo "-------------------------------------------"

# Note that we have to append || true after the grep, because a grep with count 0
# will return an exit status of 1, which will cause the script to abort with /bin/bash -e.
stirling_wrapper_pod_count=$(kubectl get pods -n "${NAMESPACE}" 2> /dev/null | grep -c ^stirling-wrapper || true)
if [ "$stirling_wrapper_pod_count" -ne 0 ]; then
  make delete_stirling_daemonset
  sleep 5
fi

echo ""
echo "-------------------------------------------"
echo "Deploying stirling_wrapper"
echo "-------------------------------------------"

make deploy_stirling_daemonset

if [ "$T" -eq 0 ]; then
  echo "Deployed stirling_wrapper with no time limit."
  echo "Run \`make delete_stirling_daemonset\` or \`kubectl delete -n pl-${USER}\` when done."
  exit 0
fi

echo ""
echo "-------------------------------------------"
echo "Waiting ${T} seconds to collect data"
echo "-------------------------------------------"

sleep "$T"

echo ""
echo "-------------------------------------------"
echo "Listing pods"
echo "-------------------------------------------"

kubectl get pods -n "${NAMESPACE}" | grep ^stirling-wrapper
pods=$(kubectl get pods -n "${NAMESPACE}" 2> /dev/null | grep ^stirling-wrapper | grep Running | cut -f1 -d' ')

echo ""
echo "-------------------------------------------"
echo "Collecting logs"
echo "-------------------------------------------"

LOGDIR=logs
mkdir -p "${LOGDIR}"

rm -rf ${LOGDIR:?}/*

timestamp=$(date +%s)
for pod in $pods; do
  # xargs removes the leading and trailing white spaces.
  node_name="$(kubectl get pod "${pod}" -n "${NAMESPACE}" -o=custom-columns=:.spec.nodeName | xargs)"
  filename="${LOGDIR}/log$timestamp.${pod}.${node_name}"
  kubectl logs -n "${NAMESPACE}" "${pod}" > "${filename}"
  echo "${scriptdir}/${filename}"
  if [[ "${OPEN_LOGS}" == true ]]; then
    less "${filename}"
  fi
done

echo ""
echo "-------------------------------------------"
echo "Cleaning up (deleting pods)"
echo "-------------------------------------------"

make delete_stirling_daemonset
