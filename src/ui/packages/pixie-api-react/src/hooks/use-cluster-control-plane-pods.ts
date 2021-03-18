import { useQuery } from '@apollo/client/react';
import { CLUSTER_QUERIES, GQLClusterInfo } from '@pixie-labs/api';
// noinspection ES6PreferShortImport
import { ImmutablePixieQueryResult } from '../utils/types';

/**
 * Retrieves a listing of control planes for currently-available clusters
 *
 * Usage:
 * ```
 * const [clusters, loading, error] = useClusterControlPlanePods();
 * ```
 */
export function useClusterControlPlanePods(): ImmutablePixieQueryResult<GQLClusterInfo[]> {
  // TODO(nick): This doesn't get the entire GQLClusterInfo, nor does useListClusters. Use Pick<...> (with nesting).
  const { data, loading, error } = useQuery<{ clusters: GQLClusterInfo[] }>(
    CLUSTER_QUERIES.GET_CLUSTER_CONTROL_PLANE_PODS,
  );

  return [data?.clusters, loading, error];
}
