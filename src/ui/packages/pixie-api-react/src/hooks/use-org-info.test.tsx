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
import { ORG_QUERIES } from '@pixie-labs/api';
import { MockedProvider } from '@apollo/client/testing';
import { act, render } from '@testing-library/react';
import { ApolloError } from '@apollo/client';
import { useOrgInfo } from './use-org-info';
import { itPassesBasicHookTests, MOCKED_PROVIDER_DEFAULT_SETTINGS } from '../testing/hook-testing-boilerplate';
import { wait } from '../testing/utils';

describe('useOrgInfo hook for org data', () => {
  const good = [
    {
      request: {
        query: ORG_QUERIES.GET_ORG_INFO,
      },
      result: {
        data: {
          org: {
            id: 'pixie',
            enableApprovals: true,
            name: 'Pixie Labs',
          },
        },
      },
    },
    {
      request: {
        query: ORG_QUERIES.SET_ORG_INFO,
        variables: {
          id: 'pixie',
          enableApprovals: false,
        },
      },
      result: { data: { mock: true } },
    },
  ];

  const bad = Object.values(ORG_QUERIES).map((v) => ({
    request: { query: v },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }));

  const Consumer = ({ action }: { action: (data: ReturnType<typeof useOrgInfo>) => Promise<any> }) => {
    const [actionPromise, setActionPromise] = React.useState<Promise<any>>(null);
    const [payload, setPayload] = React.useState<boolean>(null);
    const result = useOrgInfo();

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

  const getConsumer = (mocks, action: (data: ReturnType<typeof useOrgInfo>) => Promise<any>) => {
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
      const [{ org }, loading, error] = useOrgInfo();
      return { payload: org, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.org,
  });

  it('updates org info', async () => {
    const consumer = getConsumer(good, ([{ updateOrgInfo }]) => updateOrgInfo('pixie', false));
    // Wait once for updateOrgIfno to settle, and a second time for Consumer to settle.
    await wait();
    await wait();
    // updateOrgInfo returns Promise<void>, so we expect to see an empty string.
    expect(consumer.baseElement.textContent).toBe('');
  });
});
