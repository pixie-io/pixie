import { CLUSTER_QUERIES } from '@pixie/api';
import { ApolloError } from '@apollo/client';
import { itPassesBasicHookTests } from 'testing/hook-testing-boilerplate';
import { useListClusters } from './use-list-clusters';

describe('useListClusters hook for fetching available clusters', () => {
  const good = [{
    request: {
      query: CLUSTER_QUERIES.LIST_CLUSTERS,
    },
    result: {
      data: {
        clusters: {
          id: 'foo',
          clusterUID: 'abc-def',
          clusterName: 'Foo',
          prettyClusterName: 'Foo Erikson',
          status: 'Testing',
          lastHeartbeatMs: 0,
          numNodes: 1,
          numInstrumentedNodes: 1,
          vizierVersion: 'Infinity',
          vizierConfig: {
            passthroughEnabled: true,
          },
        },
      },
    },
  }];

  const bad = [{
    request: { query: CLUSTER_QUERIES.LIST_CLUSTERS },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  itPassesBasicHookTests({
    happyMocks: good,
    sadMocks: bad,
    useHookUnderTest: () => {
      const [clusters, loading, error] = useListClusters();
      return { payload: clusters, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.clusters,
  });
});
