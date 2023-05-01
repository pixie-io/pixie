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

# Removing member from cluster
if [ -n "${MEMBER_HASH}" ]; then
  echo "Removing ${HOSTNAME} from etcd cluster"
  remove_member
fi
