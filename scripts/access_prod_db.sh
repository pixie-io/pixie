#!/bin/bash -e

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

DB="pl_prod"

usage() {
    echo "Usage: $0 [-p] [-s]"
    echo " -p : Log into the prod db"
    echo " -s : Log into the staging db"
}

if [ $# -gt 1 ]; then
usage
exit
fi

while test $# -gt 0; do
  case "$1" in
    -p) DB="pl_prod"
        shift
        ;;
    -s) DB="pl_staging"
        shift
        ;;
    *)  usage ;;
  esac
done


# Running this script requires access to the "prod-ro" (prod-readonly) namespace in the prod cluster.
POD_NAME=$(kubectl get pod --namespace prod-ro \
    --selector="name=db-reader" --output jsonpath='{.items[0].metadata.name}')

kubectl exec -it "$POD_NAME" -n prod-ro -- bash -c \
"psql postgresql://$(kubectl get secret pl-db-ro-secrets -n prod-ro -o json | \
jq -r '.data."PL_POSTGRES_USERNAME"'  | base64 --decode):$(kubectl get secret pl-db-ro-secrets  -n prod-ro  -o json | \
jq -r '.data."PL_POSTGRES_PASSWORD"'  | base64 --decode)@localhost:5432/$DB"
