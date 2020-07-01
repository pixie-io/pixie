import { formatCluster } from './clusters-list';

describe('formatCluster', () => {
  it('correctly formats cluster info', () => {
    const clusterResults = [
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        clusterName: '123',
        prettyClusterName: 'pretty-123',
        status: 'CS_HEALTHY',
        clusterVersion: 'versionABC',
        vizierVersion: '0.2.4-pre-master.64',
        vizierConfig: {
          passthroughEnabled: false,
        },
        lastHeartbeatMs: 32349024.02,
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        clusterName: '456',
        prettyClusterName: 'pretty-456',
        status: 'CS_UPDATE_FAILED',
        clusterVersion: 'versionDEF',
        vizierVersion: '0.2.4+Distribution.d98403c.20200515173726.1',
        vizierConfig: {
          passthroughEnabled: true,
        },
        lastHeartbeatMs: 24.92,
      },
    ];
    expect(clusterResults.map((cluster) => formatCluster(cluster))).toStrictEqual([
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        name: '123',
        prettyName: 'pretty-123',
        status: 'HEALTHY',
        statusGroup: 'healthy',
        clusterVersion: 'versionABC',
        vizierVersion: '0.2.4-pre-master.64',
        vizierVersionShort: '0.2.4-pre-master.64',
        mode: 'Direct',
        lastHeartbeat: '8 hours 59 min 9 sec ago',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        name: '456',
        prettyName: 'pretty-456',
        status: 'UPDATE_FAILED',
        statusGroup: 'unhealthy',
        clusterVersion: 'versionDEF',
        vizierVersion: '0.2.4+Distribution.d98403c.20200515173726.1',
        vizierVersionShort: '0.2.4',
        mode: 'Passthrough',
        lastHeartbeat: '0 sec ago',
      },
    ]);
  });
});
