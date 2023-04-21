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

# shellcheck source=tools/docker/etcd_image/lib.sh
. /etc/etcd/scripts/lib.sh

EXISTING="$(num_existing)"

# Re-joining after failure?
if [ -n "${MEMBER_HASH}" ]; then
  echo "Re-joining member ${HOSTNAME}"
  remove_member
fi

if [ "${EXISTING}" -gt 0 ]; then
  while true; do
    echo "Waiting for ${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE} to come up"
    ping -W 1 -c 1 "${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}" > /dev/null && break
    sleep 1s
  done

  if ! etcdctl \
      --cert=/etc/etcdtls/client/etcd-tls/etcd-client.crt \
      --key=/etc/etcdtls/client/etcd-tls/etcd-client.key \
      --cacert=/etc/etcdtls/client/etcd-tls/etcd-client-ca.crt \
      --endpoints="$(eps)" \
      member add "${HOSTNAME}" \
      --peer-urls="https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2380" | \
      grep "^ETCD_" > "${DATA_DIR}/new_member_envs"; then

    echo "Member add ${HOSTNAME} error"
    rm -f "${DATA_DIR}/new_member_envs"
    exit 1
  fi

  cat "${DATA_DIR}/new_member_envs"
  # shellcheck disable=SC1091
  . "${DATA_DIR}/new_member_envs"

  exec etcd --name "${HOSTNAME}" \
      --initial-advertise-peer-urls "https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2380" \
      --listen-peer-urls https://0.0.0.0:2380 \
      --listen-client-urls https://0.0.0.0:2379 \
      --advertise-client-urls "https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2379" \
      --data-dir "${DATA_DIR}/default.etcd" \
      --initial-cluster "${ETCD_INITIAL_CLUSTER}" \
      --initial-cluster-state "${ETCD_INITIAL_CLUSTER_STATE}" \
      --peer-client-cert-auth=true \
      --peer-trusted-ca-file=/etc/etcdtls/member/peer-tls/peer-ca.crt \
      --peer-cert-file=/etc/etcdtls/member/peer-tls/peer.crt \
      --peer-key-file=/etc/etcdtls/member/peer-tls/peer.key \
      --client-cert-auth=true \
      --trusted-ca-file=/etc/etcdtls/member/server-tls/server-ca.crt \
      --cert-file=/etc/etcdtls/member/server-tls/server.crt \
      --key-file=/etc/etcdtls/member/server-tls/server.key \
      --max-request-bytes 2000000 \
      --max-wals 1 \
      --max-snapshots 1 \
      --quota-backend-bytes 8589934592 \
      --snapshot-count 5000
fi

for i in $(seq 0 "$((INITIAL_CLUSTER_SIZE - 1))"); do
  while true; do
    echo "Waiting for ${CLUSTER_NAME}-${i}.${CLUSTER_NAME}.${POD_NAMESPACE} to come up"
    ping -W 1 -c 1 "${CLUSTER_NAME}-${i}.${CLUSTER_NAME}.${POD_NAMESPACE}" > /dev/null && break
    sleep 1s
  done
done

echo "Joining member ${HOSTNAME}"
exec etcd --name "${HOSTNAME}" \
    --initial-advertise-peer-urls "https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2380" \
    --listen-peer-urls https://0.0.0.0:2380 \
    --listen-client-urls https://0.0.0.0:2379 \
    --advertise-client-urls "https://${HOSTNAME}.${CLUSTER_NAME}.${POD_NAMESPACE}.svc:2379" \
    --initial-cluster-token pl-etcd-cluster-1 \
    --data-dir "${DATA_DIR}/default.etcd" \
    --initial-cluster "$(initial_peers)" \
    --initial-cluster-state new \
    --peer-client-cert-auth=true \
    --peer-trusted-ca-file=/etc/etcdtls/member/peer-tls/peer-ca.crt \
    --peer-cert-file=/etc/etcdtls/member/peer-tls/peer.crt \
    --peer-key-file=/etc/etcdtls/member/peer-tls/peer.key \
    --client-cert-auth=true \
    --trusted-ca-file=/etc/etcdtls/member/server-tls/server-ca.crt \
    --cert-file=/etc/etcdtls/member/server-tls/server.crt \
    --key-file=/etc/etcdtls/member/server-tls/server.key \
    --max-request-bytes 2000000 \
    --max-wals 1 \
    --max-snapshots 1 \
    --quota-backend-bytes 8589934592 \
    --snapshot-count 5000
