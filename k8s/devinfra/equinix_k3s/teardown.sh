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

eip="${EIP:?}"
for server_ip in "${servers[@]}"
do
  ssh "root@${server_ip}" ip addr del "${eip}/32" dev lo || true
  ssh "root@${server_ip}" /usr/local/bin/k3s-uninstall.sh || true
done
for agent_ip in "${agents[@]}"
do
  ssh "root@${agent_ip}" /usr/local/bin/k3s-uninstall.sh
done
