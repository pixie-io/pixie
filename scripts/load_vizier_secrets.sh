#!/usr/bin/env bash

# Assume pl namespace by default.
namespace=pl
if [ "$#" -eq 1 ]; then
  namespace=$1
fi

workspace=$(bazel info workspace 2> /dev/null)

cd ${workspace}/src/utils/pixie_cli/
bazel run :px -- install-certs --namespace="$namespace" 2>/dev/null
