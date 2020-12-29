#!/bin/bash -ex

# This script updates service_tls_certs for the provided cluster

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-n k8s namespace] [-d path to the directory of the service_tls_certs.yaml file]" 1>&2;
  exit 1;
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "n:d:h" opt; do
    case ${opt} in
      n)
        namespace=$OPTARG
        ;;
      d)
        dir=$OPTARG
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

if [ -z "${namespace}" ]; then
  echo "Namespace (-n) must be provided."
  exit 1
fi

if [ -z "${dir}" ]; then
  echo "Path to the directory service_tls_certs.yaml file (-d) must be provided."
  exit 1
fi

workspace=$(bazel info workspace 2> /dev/null)
pushd "${dir}"
bazel run //src/pixie_cli:px -- install-certs --namespace="$namespace"
kubectl -n "${namespace}" get secrets service-tls-certs -o yaml | \
  python "${workspace}/scripts/decode_yaml_secret.py" > out.unenc.yaml
sops --encrypt out.unenc.yaml > service_tls_certs.yaml
rm out.unenc.yaml
popd
