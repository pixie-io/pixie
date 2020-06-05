import {selectCluster} from './vizier';

describe('selectCluster', () => {
  it('should select the right default cluster', () => {
    const clusters = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        status: 'CS_DISCONNECTED',
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        status: 'CS_HEALTHY',
      },
      {
        id: '5fffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        status: 'CS_HEALTHY',
      },
    ];

    expect(selectCluster(clusters)).toStrictEqual({
      'id': 'efffeb73-d19c-4630-af63-f75c3761bab4',
      clusterName: 'foobar2',
      status: 'CS_HEALTHY',
    });
  });

  it('should select the right default cluster with no CS_HEALTHY clusters', () => {
    const clusters = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        status: 'CS_DISCONNECTED',
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        status: 'CS_UPDATING',
      },
      {
        id: '6eefeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        status: 'CS_CONNECTED',
      },
    ];

    expect(selectCluster(clusters)).toStrictEqual({
      id: '6eefeb73-d19c-4630-af63-f75c3761bab4',
      clusterName: 'foobar2',
      status: 'CS_CONNECTED',
    });
  });
});
