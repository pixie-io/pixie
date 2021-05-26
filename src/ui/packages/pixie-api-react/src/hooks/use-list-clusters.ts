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

import { useQuery } from '@apollo/client/react';
import { CLUSTER_QUERIES, GQLClusterInfo } from '@pixie-labs/api';
// noinspection ES6PreferShortImport
import { ImmutablePixieQueryResult } from '../utils/types';

/**
 * Retrieves a listing of currently-available clusters to run Pixie scripts on.
 *
 * Usage:
 * ```
 * const [clusters, loading, error] = useListClusters();
 * ```
 */
export function useListClustersVerbose(): ImmutablePixieQueryResult<GQLClusterInfo[]> {
  // TODO(nick): This doesn't get the entire GQLClusterInfo, nor does useClusterControlPlanePods. Use Pick<...>.
  const { data, loading, error } = useQuery<{ clusters: GQLClusterInfo[] }>(
    CLUSTER_QUERIES.LIST_CLUSTERS_VERBOSE,
    { pollInterval: 2500, fetchPolicy: 'network-only' },
  );

  return [data?.clusters, loading, error];
}

export function useClusterPassthroughInfo(id: string): ImmutablePixieQueryResult<GQLClusterInfo> {
  const { data, loading, error } = useQuery<{ cluster: GQLClusterInfo }, { id: string }>(
    CLUSTER_QUERIES.CLUSTER_PASSTHROUGH_INFO,
    { variables: { id } },
  );

  return [data?.cluster, loading, error];
}

export function useListClusters(): ImmutablePixieQueryResult<GQLClusterInfo[]> {
  // TODO(nick): This doesn't get the entire GQLClusterInfo, nor does useClusterControlPlanePods. Use Pick<...>.
  const { data, loading, error } = useQuery<{ clusters: GQLClusterInfo[] }>(
    CLUSTER_QUERIES.LIST_CLUSTERS,
    { pollInterval: 2500, fetchPolicy: 'network-only' },
  );

  return [data?.clusters, loading, error];
}
