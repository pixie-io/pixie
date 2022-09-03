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

usage() {
  echo "Usage: $0 [-p] [-s] [-w]"
  echo " -p : Log into the prod db"
  echo " -s : Log into the staging db"
  echo " -t : Log into the testing db"
  echo " -w : Log into the db with write access"
  exit 1
}

set_default_values() {
  DB=pl_prod
  READER_NS=prod-ro
  NS=plc

  WRITE=false
}

parse_args() {
  set_default_values

  local OPTIND
  while getopts "pstw" opt; do
    case ${opt} in
      p)
        ;;
      s)
        DB=pl_staging
        READER_NS=staging-ro
        SECRET_NS=staging-ro
        NS=plc-staging
        ;;
      t)
        DB=pl_testing
        READER_NS=testing-ro
        SECRET_NS=testing-ro
        NS=plc-testing
        ;;
      w)
        WRITE=true
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND - 1))
}

parse_args "$@"

if [[ $WRITE == "true" ]]; then
  SECRET_NS="${NS}"
  SECRET_NAME=pl-db-secrets
else
  SECRET_NS="${READER_NS}"
  SECRET_NAME=pl-db-ro-secrets
fi

# Running this script requires access to the "prod-ro" (prod-readonly) namespace in the prod cluster.
POD_NAME=$(kubectl get pod --namespace $READER_NS \
    --selector="name=db-reader" --output jsonpath='{.items[0].metadata.name}')

kubectl exec -it "$POD_NAME" -n $READER_NS -c psql -- bash -c \
"psql postgresql://$(kubectl get secret $SECRET_NAME -n $SECRET_NS -o json | \
jq -r '.data."PL_POSTGRES_USERNAME"'  | base64 --decode):$(kubectl get secret $SECRET_NAME  -n $SECRET_NS  -o json | \
jq -r '.data."PL_POSTGRES_PASSWORD"'  | base64 --decode)@localhost:5432/$DB"
