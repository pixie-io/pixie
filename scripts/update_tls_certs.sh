#!/bin/bash -ex

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

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

pushd "${dir}"
bazel run //src/pixie_cli:px -- create-cloud-certs --namespace="$namespace" > certs.yaml
sops --encrypt certs.yaml > service_tls_certs.yaml
rm certs.yaml
popd
