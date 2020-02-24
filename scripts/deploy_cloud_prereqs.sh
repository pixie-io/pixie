#!/usr/bin/env bash

if [ "$#" -ne 2 ]; then
  echo "This script requires exactly two arguments: <namespace> <secret type : dev, prod, etc.>"
fi
namespace=$1
secret_type=$2

workspace=$(bazel info workspace 2> /dev/null)
# shellcheck source=scripts/script_utils.sh
source "${workspace}"/scripts/script_utils.sh

ensure_namespace "${namespace}"

"${workspace}"/scripts/load_cloud_secrets.sh "${namespace}" "${secret_type}"
"${workspace}"/scripts/deploy_cloud_deps.sh "${secret_type}"
