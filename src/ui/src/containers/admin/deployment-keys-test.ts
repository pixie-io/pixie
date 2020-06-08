import {formatDeploymentKey} from './deployment-keys';

describe('formatDeploymentKey', () => {
  it('correctly formats deployment keys', () => {
    const deploymentKeyResults = [
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        createdAtMs: new Date(new Date().getTime() - 1000*60*60*24*2), // 2 days ago
        key: 'foobar1',
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        createdAtMs: new Date(new Date().getTime() - 1000*60*60*3), // 3 hours ago
        key: 'foobar2',
        desc: 'abcd2',
      },
    ];
    expect(deploymentKeyResults.map(key => formatDeploymentKey(key))).toStrictEqual([
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        createdAt: '2 days ago',
        key: 'foobar1',
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        createdAt: 'about 3 hours ago',
        key: 'foobar2',
        desc: 'abcd2',
      }
    ]);
  });
});
