import { useQuery } from '@apollo/client/react';
import { CLUSTER_QUERIES, GQLClusterInfo } from '@pixie/api';
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
export function useListClusters(): ImmutablePixieQueryResult<GQLClusterInfo[]> {
  // TODO(nick): This doesn't get the entire GQLClusterInfo, nor does useClusterControlPlanePods. Use Pick<...>.
  const { data, loading, error } = useQuery<{ clusters: GQLClusterInfo[] }>(
    CLUSTER_QUERIES.LIST_CLUSTERS,
    { pollInterval: 2500, fetchPolicy: 'network-only' },
  );

  return [data?.clusters, loading, error];
}
