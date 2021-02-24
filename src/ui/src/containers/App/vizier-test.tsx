import { GQLClusterInfo as Cluster, GQLClusterStatus as ClusterStatus } from '@pixie/api';
import { selectCluster } from './cluster-info';

describe('selectCluster', () => {
  const commonClusterProps = {
    clusterVersion: '1.0.0',
    lastHeartbeatMs: 0,
    vizierConfig: {},
    controlPlanePodStatuses: [],
    numNodes: 1,
    numInstrumentedNodes: 1,
  };

  it('should select the right default cluster', () => {
    const clusters: Cluster[] = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterUID: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        prettyClusterName: 'Foobar 1',
        status: ClusterStatus.CS_DISCONNECTED,
        ...commonClusterProps,
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        prettyClusterName: 'Foobar 2',
        status: ClusterStatus.CS_HEALTHY,
        ...commonClusterProps,
      },
      {
        id: '5fffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: '5fffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        prettyClusterName: 'Foobar 3',
        status: ClusterStatus.CS_HEALTHY,
        ...commonClusterProps,
      },
    ];

    expect(selectCluster(clusters)).toStrictEqual({ ...clusters[1] });
  });

  it('should select the right default cluster with no ClusterStatus.CS_HEALTHY clusters', () => {
    const clusters: Cluster[] = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterUID: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        prettyClusterName: 'Foobar 1',
        status: ClusterStatus.CS_DISCONNECTED,
        ...commonClusterProps,
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        prettyClusterName: 'Foobar 3',
        status: ClusterStatus.CS_UPDATING,
        ...commonClusterProps,
      },
      {
        id: '6eefeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: '6eefeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        prettyClusterName: 'Foobar 2',
        status: ClusterStatus.CS_CONNECTED,
        ...commonClusterProps,
      },
    ];

    expect(selectCluster(clusters)).toStrictEqual({ ...clusters[2] });
  });
});
