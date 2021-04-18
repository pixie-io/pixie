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

endpoint="https://localhost:2379"
cert="/clientcerts/etcd-client.crt"
key="/clientcerts/etcd-client.key"
cacert="/clientcerts/etcd-client-ca.crt"

kubectl exec -t -i \
 $(kubectl get pod --namespace px \
    --selector="vizier-dep=etcd" --output jsonpath='{.items[0].metadata.name}') \
 -n=pl -- /bin/sh -c \
 "ETCDCTL_API=3 etcdctl \
  --endpoints=$endpoint \
  --cert=$cert \
  --key=$key \
  --cacert=$cacert \
  $1"
