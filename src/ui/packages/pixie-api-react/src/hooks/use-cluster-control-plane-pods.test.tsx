import { CLUSTER_QUERIES } from '@pixie/api';
import { ApolloError } from '@apollo/client';
import { itPassesBasicHookTests } from 'testing/hook-testing-boilerplate';
import { useClusterControlPlanePods } from './use-cluster-control-plane-pods';

describe('useClusterControlPlanePods hook for fetching control planes', () => {
  const good = [{
    request: {
      query: CLUSTER_QUERIES.GET_CLUSTER_CONTROL_PLANE_PODS,
    },
    result: {
      data: {
        clusters: {
          clusterName: 'Foo',
          controlPlanePodStatuses: [{
            name: 'Bar',
            status: 'Good, thanks for asking.',
            message: "It's a nice day out!",
            reason: "It's sunny and warm.",
            containers: [{
              name: 'Baz',
              state: 'Also good.',
              message: 'Say hello to Bar for me!',
              reason: 'Just because.',
            }],
          }],
        },
      },
    },
  }];

  const bad = [{
    request: { query: CLUSTER_QUERIES.GET_CLUSTER_CONTROL_PLANE_PODS },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  itPassesBasicHookTests({
    happyMocks: good,
    sadMocks: bad,
    useHookUnderTest: () => {
      const [clusters, loading, error] = useClusterControlPlanePods();
      return { payload: clusters, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.clusters,
  });
});
