#!/bin/bash
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

gcloud_project="pl-pixies"
gcloud_region="us-west1"
gcloud_network="dev"
namespace="px-perf"
resource_name="px-perf"
# Should only need to change things above this line to deploy to a non-(Pixie core team) environment.
resource_name_underscores="${resource_name//-/_}"

workspace=$(git rev-parse --show-toplevel)
credentials_path="${workspace}/private/credentials/dev_infra/perf_tool"
k8s_path="${workspace}/src/e2e_test/perf_tool/backend/k8s"

# shellcheck source=src/e2e_test/perf_tool/scripts/lib.sh
source "${workspace}/src/e2e_test/perf_tool/scripts/lib.sh"

create_service_tls_certs
create_postgres_instance
create_bq_dataset_service_account
