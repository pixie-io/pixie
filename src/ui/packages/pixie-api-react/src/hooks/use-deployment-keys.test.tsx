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

import * as React from 'react';
import { DEPLOYMENT_KEY_QUERIES } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { act, render } from '@testing-library/react';
import { MockedProvider } from '@apollo/client/testing';
import { itPassesBasicHookTests, MOCKED_PROVIDER_DEFAULT_SETTINGS } from '../testing/hook-testing-boilerplate';
import { wait } from '../testing/utils';
import { useDeploymentKeys } from './use-deployment-keys';

describe('useDeploymentKeys hook for creating, listing, and deleting deployment keys', () => {
  const good = [
    {
      request: {
        query: DEPLOYMENT_KEY_QUERIES.LIST_DEPLOYMENT_KEYS,
      },
      result: {
        data: {
          deploymentKeys: [{
            id: 'foo', key: 'bar-baz', desc: 'Buzzer', createdAtMs: 0,
          }],
        },
      },
    },
    // The first LIST_DEPLOYMENT_KEYS is for the standard tests; the second is for createDeploymentKey
    {
      request: {
        query: DEPLOYMENT_KEY_QUERIES.LIST_DEPLOYMENT_KEYS,
      },
      result: {
        data: {
          deploymentKeys: [{
            id: 'foo', key: 'bar-baz', desc: 'Buzzer', createdAtMs: 0,
          }],
        },
      },
    },
    {
      request: {
        query: DEPLOYMENT_KEY_QUERIES.CREATE_DEPLOYMENT_KEY,
      },
      result: {
        data: {
          CreateDeploymentKey: { id: 'bar' },
        },
      },
    },
    {
      request: {
        query: DEPLOYMENT_KEY_QUERIES.DELETE_DEPLOYMENT_KEY,
        variables: {
          id: 'foo',
        },
      },
      result: { data: { mock: true } },
    },
  ];

  const bad = Object.values(DEPLOYMENT_KEY_QUERIES).map((v) => ({
    request: { query: v },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }));

  const Consumer = ({ action }: { action: (data: ReturnType<typeof useDeploymentKeys>) => Promise<any> }) => {
    const [actionPromise, setActionPromise] = React.useState<Promise<any>>(null);
    const [payload, setPayload] = React.useState<boolean>(null);
    const result = useDeploymentKeys();

    React.useEffect(() => {
      const [,loading] = result;
      if (loading || actionPromise) return;
      setActionPromise(action(result).then((p) => {
        act(() => setPayload(p));
      }));
      // Stringifying the result because its first member's identity changes between calls to useMutation.
      // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [action, JSON.stringify(result)]);

    return <>{ JSON.stringify(payload) }</>;
  };

  const getConsumer = (mocks, action: (data: ReturnType<typeof useDeploymentKeys>) => Promise<any>) => {
    let consumer;
    act(() => {
      consumer = render(
        <MockedProvider mocks={mocks} addTypename={false} defaultOptions={MOCKED_PROVIDER_DEFAULT_SETTINGS}>
          <Consumer action={action} />
        </MockedProvider>,
      );
    });
    // noinspection JSUnusedAssignment
    return consumer;
  };

  itPassesBasicHookTests({
    happyMocks: good,
    sadMocks: bad,
    useHookUnderTest: () => {
      const [{ deploymentKeys }, loading, error] = useDeploymentKeys();
      return { payload: deploymentKeys, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.deploymentKeys,
  });

  it('creates deployment keys', async () => {
    const consumer = getConsumer(good, ([{ createDeploymentKey }]) => createDeploymentKey());
    // Wait once for useDeploymentKeys to settle, and a second time for Consumer to settle.
    await wait();
    await wait();
    expect(consumer.baseElement.textContent).toBe('"bar"');
  });

  it('deletes deployment keys', async () => {
    const consumer = getConsumer(good, ([{ deleteDeploymentKey }]) => deleteDeploymentKey('foo'));
    // Wait once for useDeploymentKeys to settle, and a second time for Consumer to settle.
    await wait();
    await wait();
    // deleteDeploymentKey returns Promise<void>, so we expect to see an empty string.
    expect(consumer.baseElement.textContent).toBe('');
  });
});
