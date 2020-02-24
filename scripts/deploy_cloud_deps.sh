#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then
  echo "This script requires exactly one argument: <deploy_env : dev, prod, etc.>"
  exit 1
fi
deploy_env=$1

workspace=$(bazel info workspace 2> /dev/null)

# shellcheck source=scripts/script_utils.sh
source "${workspace}"/scripts/script_utils.sh

cloud_deps_deploy() {
  # Can't use kubectl -k because bundled kustomize is v2.0.3 as of kubectl v1.16, vs kustomize which is v3.5.4
  kustomize build "${workspace}"/k8s/cloud_deps/"${deploy_env}" | kubectl apply -f -
}

retry cloud_deps_deploy 5 30
