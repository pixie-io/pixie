#!/usr/bin/env bash

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

set -e

if [ "$#" -ne 2 ]; then
  echo "This script requires exactly two argument: <namespace> <secret type : dev, prod, etc.>"
fi
namespace=$1
secret_type=$2

workspace=$(git rev-parse --show-toplevel)
credentials_path=${workspace}/private/credentials/k8s/${secret_type}
monitoring_path=${workspace}/private/credentials/k8s/monitoring/${secret_type}

if [ ! -d "${credentials_path}" ]; then
  echo "Credentials path \"${credentials_path}\" does not exist. Did you slect the right secret type?"
  exit 1
fi

shopt -s nullglob
# Apply configs.
for yaml in "${credentials_path}"/configs/*.yaml; do
  echo "Loading: ${yaml}"
  sops --decrypt "${yaml}" | kubectl apply -n "${namespace}" -f -
done
# Apply secrets.
for yaml in "${credentials_path}"/*.yaml; do
  echo "Loading: ${yaml}"
  sops --decrypt "${yaml}" | kubectl apply -n "${namespace}" -f -
done

if [ ! -d "${monitoring_path}" ]; then
  exit 0
fi
