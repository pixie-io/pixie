#!/usr/bin/env bash

workspace=$(bazel info workspace 2> /dev/null)

usage() {
  echo "Usage: $0 [-l] [<demo application>]"
  echo " -l Additionally deploy load generator (if available)"
  echo ""
  echo "List of supported demo applications:"
  echo "  sockshop (default)"
  echo "  hipster-shop"
  exit
}

parse_args() {
  # Default values
  TARGET="sockshop"
  DEPLOY_LOAD_GEN=false

  local OPTIND
  while getopts "lh" opt; do
    case ${opt} in
      l) DEPLOY_LOAD_GEN=true;;
      h) usage;;
      *) usage;;
    esac
  done
  shift $((OPTIND-1))

  if [ "$#" -eq 1 ]; then
    TARGET=$1
  elif [ "$#" -gt 1 ]; then
    usage
  fi

  echo "Demo app: $TARGET"
  echo "Load generation: $DEPLOY_LOAD_GEN"
  echo ""
}

parse_args "$@"

if [ "$TARGET" = "sockshop" ]; then
  kubectl apply -f "$workspace"/demos/applications/sockshop/kubernetes_manifests/sock-shop-ns.yaml && sleep 5
  kubectl apply -f "$workspace"/demos/applications/sockshop/kubernetes_manifests
  if [ "$DEPLOY_LOAD_GEN" = true ]; then
    kubectl apply -f "$workspace"/demos/applications/sockshop/load_generation
  fi
elif [ "$TARGET" = "hipster-shop" ]; then
  kubectl apply -f "$workspace"/demos/applications/hipster_shop/kubernetes_manifests/0000_namespace.yaml && sleep 5
  kubectl apply -f "$workspace"/demos/applications/hipster_shop/kubernetes_manifests
  if [ "$DEPLOY_LOAD_GEN" = true ]; then
    echo "WARNING: hipster-shop load generation not yet supported."
  fi
else
  echo "ERROR: $TARGET is not a valid demo app"
  usage
fi
