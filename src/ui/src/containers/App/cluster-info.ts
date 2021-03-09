import { GQLClusterStatus as ClusterStatus } from '@pixie/api';

interface ClusterInfo {
  id: string;
  clusterName: string;
  status: string;
}

// Selects based on cluster status and tiebreaks by cluster name.
export function selectCluster(clusters: ClusterInfo[]): ClusterInfo {
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
  })[0];
}
