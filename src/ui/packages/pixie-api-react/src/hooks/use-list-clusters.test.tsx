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

import { CLUSTER_QUERIES } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { itPassesBasicHookTests } from '../testing/hook-testing-boilerplate';
import { useListClusters, useListClustersVerbose } from './use-list-clusters';

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

describe('useListClustersVerbose hook for fetching available clusters', () => {
  const good = [{
    request: {
      query: CLUSTER_QUERIES.LIST_CLUSTERS_VERBOSE,
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
    request: { query: CLUSTER_QUERIES.LIST_CLUSTERS_VERBOSE },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  itPassesBasicHookTests({
    happyMocks: good,
    sadMocks: bad,
    useHookUnderTest: () => {
      const [clusters, loading, error] = useListClustersVerbose();
      return { payload: clusters, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.clusters,
  });
});
