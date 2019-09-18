#!/usr/bin/env bash

vizier_namespace=pl

workspace=$(bazel info workspace 2> /dev/null)
source ${workspace}/scripts/script_utils.sh

# Make the current user a cluster-admin
# WARNING: this is insecure.
# TODO(oazizi/philkuz): Fix when we set-up RBAC.
${workspace}/scripts/setup_cluster_role_bindings.sh

ensure_namespace ${vizier_namespace}

${workspace}/scripts/load_vizier_secrets.sh ${vizier_namespace}

${workspace}/scripts/deploy_vizier_operators.sh ${vizier_namespace}
