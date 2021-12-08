/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { GQLClusterStatus as ClusterStatus, GQLClusterInfo as Cluster } from 'app/types/schema';

// Selects a default clusterName based on the status.
export function selectClusterName(clusters: Pick<Cluster, 'clusterName' | 'status'>[]): string {
  if (clusters.length === 0) {
    return null;
  }
  // Buckets cluster states by desirability for selection.
  // 0 = most prioritized.
  const clusterStatusMap = {
    [ClusterStatus.CS_UNKNOWN]: 3,
    [ClusterStatus.CS_HEALTHY]: 0,
    [ClusterStatus.CS_UNHEALTHY]: 2,
    [ClusterStatus.CS_DISCONNECTED]: 3,
    [ClusterStatus.CS_UPDATING]: 1,
    [ClusterStatus.CS_CONNECTED]: 1,
    [ClusterStatus.CS_UPDATE_FAILED]: 2,
    [ClusterStatus.CS_DEGRADED]: 1,
  };
  const defaultStatusValue = 3;
  // Copy over in case clusters is read only.
  return clusters.slice().sort((cluster1, cluster2) => {
    const status1 = clusterStatusMap[cluster1.status] === undefined
      ? defaultStatusValue : clusterStatusMap[cluster1.status];
    const status2 = clusterStatusMap[cluster2.status] === undefined
      ? defaultStatusValue : clusterStatusMap[cluster2.status];
    if (status1 < status2) {
      return -1;
    }
    if (status1 > status2) {
      return 1;
    }
    return cluster1.clusterName < cluster2.clusterName ? -1 : 1;
  })[0]?.clusterName;
}
