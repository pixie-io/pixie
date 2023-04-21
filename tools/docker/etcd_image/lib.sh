#!/bin/sh -e

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

INITIAL_CLUSTER_SIZE="${INITIAL_CLUSTER_SIZE:?}"
CLUSTER_NAME="${CLUSTER_NAME:?}"
POD_NAMESPACE="${POD_NAMESPACE:?}"
DATA_DIR="${DATA_DIR:?}"

HOSTNAME="$(hostname)"

eps() {
  EPS=""
  for i in $(seq 0 "$((INITIAL_CLUSTER_SIZE - 1))"); do
    EPS="${EPS}${EPS:+,}https://${CLUSTER_NAME}-${i}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2379"
  done
  echo "${EPS}"
}

member_hash() {
  etcdctl \
      --cert=/etc/etcdtls/client/etcd-tls/etcd-client.crt \
      --key=/etc/etcdtls/client/etcd-tls/etcd-client.key \
      --cacert=/etc/etcdtls/client/etcd-tls/etcd-client-ca.crt \
      --endpoints="$(eps)" \
      member list | grep "https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2380" | cut -d',' -f1
}

num_existing() {
  etcdctl \
      --cert=/etc/etcdtls/client/etcd-tls/etcd-client.crt \
      --key=/etc/etcdtls/client/etcd-tls/etcd-client.key \
      --cacert=/etc/etcdtls/client/etcd-tls/etcd-client-ca.crt \
      --endpoints="$(eps)" \
      member list | wc -l
}

initial_peers() {
  PEERS=""
  for i in $(seq 0 "$((INITIAL_CLUSTER_SIZE - 1))"); do
    PEERS="${PEERS}${PEERS:+,}${CLUSTER_NAME}-${i}=https://${CLUSTER_NAME}-${i}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2380"
  done
  echo "${PEERS}"
}

MEMBER_HASH="$(member_hash)"

remove_member() {
  if etcdctl \
      --cert=/etc/etcdtls/client/etcd-tls/etcd-client.crt \
      --key=/etc/etcdtls/client/etcd-tls/etcd-client.key \
      --cacert=/etc/etcdtls/client/etcd-tls/etcd-client-ca.crt \
      --endpoints="$(eps)" \
      member remove "${MEMBER_HASH}"; then
    rm -rf "${DATA_DIR:?}/*"
    mkdir -p "${DATA_DIR:?}"
  fi
}

export HOSTNAME
export MEMBER_HASH
