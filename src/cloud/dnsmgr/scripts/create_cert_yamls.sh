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

# Script to generate certs and place them into yaml files.
workspace=$(bazel info workspace 2> /dev/null)
curdirectory=${workspace}/src/cloud/dnsmgr/scripts

if [ $# -ne 3 ]; then
  echo "Expected 3 Arguments: EMAIL and OUTDIR and NUMCERTS. Received $#."
  exit 1
fi
EMAIL=$1
OUTDIR=$2
NUMCERTS=$3
"${curdirectory}/generate_certs.sh" "${EMAIL}" "${OUTDIR}" "${NUMCERTS}"
"${curdirectory}/convert_certs_to_yaml.sh" "${OUTDIR}/certificates" "${workspace}/credentials/certs"
