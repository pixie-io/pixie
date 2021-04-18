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
