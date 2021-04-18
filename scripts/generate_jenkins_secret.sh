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

# This script can be used to generate secret tokens used by Jenkins.
# Any cluster accessed by Jenkins needs credentials to be registered
# in the UI under: https://jenkins.pixielabs.ai/credentials.

namespace=default
account_name=jenkins-robot

kubectl -n ${namespace} create serviceaccount ${account_name}

# The next line gives `${account_name}` administator permissions for this namespace.
kubectl -n ${namespace} create clusterrolebinding "${account_name}-binding" \
        --clusterrole=cluster-admin --serviceaccount="${namespace}:${account_name}"

# Get the name of the token that was automatically generated for the ServiceAccount `${account_name}`.
token_name=$(kubectl -n ${namespace} get serviceaccount "${account_name}" \
                     -o go-template --template='{{range .secrets}}{{.name}}{{"\n"}}{{end}}')

# Retrieve the token and decode it using base64.
kubectl -n ${namespace} get secrets "${token_name}" -o go-template \
        --template '{{index .data "token"}}' | base64 -d
printf "\n"
