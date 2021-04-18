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
import { API_KEY_QUERIES } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { act, render } from '@testing-library/react';
import { MockedProvider } from '@apollo/client/testing';
import { itPassesBasicHookTests, MOCKED_PROVIDER_DEFAULT_SETTINGS } from '../testing/hook-testing-boilerplate';
import { wait } from '../testing/utils';
import { useAPIKeys } from './use-api-keys';

describe('useAPIKeys hook for creating, listing, and deleting API keys', () => {
  const good = [
    {
      request: {
        query: API_KEY_QUERIES.LIST_API_KEYS,
      },
      result: {
        data: {
          apiKeys: [{
            id: 'foo', key: 'bar-baz', desc: 'Buzzer', createdAtMs: 0,
          }],
        },
      },
    },
    // The first LIST_API_KEYS is for the standard tests; the second is for createAPIKey
    {
      request: {
        query: API_KEY_QUERIES.LIST_API_KEYS,
      },
      result: {
        data: {
          apiKeys: [{
            id: 'foo', key: 'bar-baz', desc: 'Buzzer', createdAtMs: 0,
          }],
        },
      },
    },
    {
      request: {
        query: API_KEY_QUERIES.CREATE_API_KEY,
      },
      result: {
        data: {
          CreateAPIKey: { id: 'bar' },
        },
      },
    },
    {
      request: {
        query: API_KEY_QUERIES.DELETE_API_KEY,
        variables: {
          id: 'foo',
        },
      },
      result: { data: { mock: true } },
    },
  ];

  const bad = Object.values(API_KEY_QUERIES).map((v) => ({
    request: { query: v },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }));

  const Consumer = ({ action }: { action: (data: ReturnType<typeof useAPIKeys>) => Promise<any> }) => {
    const [actionPromise, setActionPromise] = React.useState<Promise<any>>(null);
    const [payload, setPayload] = React.useState<boolean>(null);
    const result = useAPIKeys();

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

  const getConsumer = (mocks, action: (data: ReturnType<typeof useAPIKeys>) => Promise<any>) => {
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
      const [{ apiKeys }, loading, error] = useAPIKeys();
      return { payload: apiKeys, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.apiKeys,
  });

  it('creates API keys', async () => {
    const consumer = getConsumer(good, ([{ createAPIKey }]) => createAPIKey());
    // Wait once for useAPIKeys to settle, and a second time for Consumer to settle.
    await wait();
    await wait();
    expect(consumer.baseElement.textContent).toBe('"bar"');
  });

  it('deletes API keys', async () => {
    const consumer = getConsumer(good, ([{ deleteAPIKey }]) => deleteAPIKey('foo'));
    // Wait once for useAPIKeys to settle, and a second time for Consumer to settle.
    await wait();
    await wait();
    // deleteAPIKey returns Promise<void>, so we expect to see an empty string.
    expect(consumer.baseElement.textContent).toBe('');
  });
});
