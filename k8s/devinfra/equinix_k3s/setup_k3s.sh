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

if [[ "$#" -ne 1 ]]; then
  echo "Usage: $0 <kubeconfig_path>"
  exit 1
fi

kubeconfig="$1"
eip="${EIP:?}"
project_id="${EQUINIX_PROJECT_ID:?}"
api_key="${EQUINIX_API_KEY:?}"
equinix_metro="${EQUINIX_METRO:?}"
eip_tag="${EIP_TAG:?}"

servers=()
agents=()
for ip in ${SERVER_IPS//,/ }
do
  servers+=("${ip}")
done

for ip in ${AGENT_IPS//,/ }
do
  agents+=("${ip}")
done

if [[ "${#servers[@]}" -eq 0 ]]; then
  echo "Need at least one server"
  exit 1
fi

internal_ip() {
  external_ip="$1"
  ssh "root@${external_ip}" "curl https://metadata.platformequinix.com/metadata" | jq -r '.network.addresses | map(select(.public==false and .management==true)) | first | .address'
}

add_eip_loopback() {
  server_ip="$1"
  ifs="$(mktemp)"
  scp "root@${server_ip}:/etc/network/interfaces" "${ifs}"

  if ! grep 'lo:0' "${ifs}"; then
    cat >>"${ifs}" <<EOF
auto lo:0
iface lo:0 inet static
  address ${eip}
  netmask 255.255.255.255
EOF
    scp "${ifs}" "root@${server_ip}:/etc/network/interfaces"
  fi
  # Sometimes we have to bring the interface down before we bring it up.
  eip_loopback_down "${server_ip}" || true
  eip_loopback_up "${server_ip}" || true
}

eip_loopback_down() {
  server_ip="$1"
  ssh "root@${server_ip}" ifdown lo:0
}

eip_loopback_up() {
  server_ip="$1"
  ssh "root@${server_ip}" ifup lo:0
}

s1="${servers[0]}"

common_k3s_args=("--disable=servicelb"
 "--disable=traefik"
 "--disable-cloud-controller"
 "--kube-apiserver-arg" "--anonymous-auth=true"
 "--kubelet-arg" "--cloud-provider=external"
 "--kubelet-arg" "--root-dir=/data/var/lib/kubelet"
)

node_k3s_args=("--node-ip" "$(internal_ip "${s1}")"
  "--node-external-ip" "${s1}"
)

add_eip_loopback "${s1}"

k3sup install \
  --ip "${s1}" \
  --tls-san "${eip}" \
  --cluster \
  --k3s-channel latest \
  --k3s-extra-args "${common_k3s_args[*]} ${node_k3s_args[*]}" \
  --local-path "${kubeconfig}" \
  --context=equinix-k3s-cluster

export KUBECONFIG="${kubeconfig}"

cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Secret
metadata:
  name: metal-cloud-config
  namespace: kube-system
stringData:
  cloud-sa.json: |
    {
      "apiKey": "${api_key}",
      "projectID": "${project_id}",
      "eipTag": "${eip_tag}",
      "eipHealthCheckUseHostIP": true,
      "loadbalancer": "kube-vip://",
      "metro": "${equinix_metro}"
    }
EOF

kubectl apply -f https://github.com/equinix/cloud-provider-equinix-metal/releases/download/v3.6.2/deployment.yaml

curl https://kube-vip.io/manifests/rbac.yaml | kubectl apply -f -

KVVERSION=$(curl -sL https://api.github.com/repos/kube-vip/kube-vip/releases | jq -r ".[0].name")

kube-vip() {
  docker run --network host --rm "ghcr.io/kube-vip/kube-vip:$KVVERSION" "$@"
}

kube-vip manifest daemonset \
  --interface lo \
  --services \
  --bgp \
  --annotations metal.equinix.com \
  --inCluster | kubectl apply -f -

sed -i "s/${s1}/${eip}/g" "${kubeconfig}"

server_join() {
  server_ip="$1"
  add_eip_loopback "${server_ip}"
  # We need the eip loopback interface to be down to join the cluster using the eip.
  eip_loopback_down "${server_ip}"
  node_k3s_args=("--node-ip" "$(internal_ip "${server_ip}")"
    "--node-external-ip" "${server_ip}"
  )
  k3sup join \
    --ip "${server_ip}" \
    --server \
    --server-ip "${eip}" \
    --k3s-extra-args "${common_k3s_args[*]} ${node_k3s_args[*]}" \
    --k3s-channel latest
}

agent_join() {
  agent_ip="$1"
  k3s_args="--node-ip $(internal_ip "${agent_ip}") --node-external-ip ${agent_ip}"
  k3sup join \
    --ip "${agent_ip}" \
    --server-ip "${eip}" \
    --k3s-extra-args "${k3s_args}" \
    --k3s-channel latest
}

for server_ip in "${servers[@]:1}"
do
  server_join "${server_ip}"
  # wait for k3s to join before bringing up the loopback eip addr.
  eip_loopback_up "${server_ip}"
done

for agent_ip in "${agents[@]}"
do
  agent_join "${agent_ip}"
done
