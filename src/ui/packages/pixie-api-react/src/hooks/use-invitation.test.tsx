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
import { USER_QUERIES } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { MockedProvider } from '@apollo/client/testing';
import { render } from '@testing-library/react';
import { useInvitation } from 'hooks/use-invitation';
import { GQLUserInvite } from '@pixie-labs/api/dist/types/schema';
import { wait } from '../testing/utils';

describe('useAutocomplete to suggest available entities from user input', () => {
  const good = () => [
    {
      request: {
        query: USER_QUERIES.INVITE_USER,
        variables: {
          firstName: 'Test',
          lastName: 'Exampleton',
          email: 'test@example.com',
        },
      },
      result: {
        data: {
          InviteUser: {
            email: 'test@example.com',
            inviteLink: 'https://invite.link/some-guid',
          },
        },
      },
    },
  ];

  const bad = () => [
    {
      request: good()[0].request,
      error: new ApolloError({ errorMessage: 'Request failed!' }),
    },
  ];

  /*
   * Not using itPassesBasicHookTests, because this hook behaves much differently. Unlike most of the direct
   * Apollo query hooks, this one provides a callback that in turn executes (memoized) queries and transforms
   * those into Promise objects.
   */

  const Consumer: React.FC = () => {
    const [result, setResult] = React.useState<GQLUserInvite>(null);
    const [error, setError] = React.useState<ApolloError>(null);
    const inviter = useInvitation();
    React.useMemo(() => {
      inviter('Test', 'Exampleton', 'test@example.com').then(setResult).catch(setError);
    }, [inviter]);

    return <>{ JSON.stringify(result ?? error?.message) }</>;
  };

  const getConsumer = (mocks) => render(
    <MockedProvider mocks={mocks} addTypename={false}>
      <Consumer />
    </MockedProvider>,
  );

  it('Provides a promise of an eventual invitation link', async () => {
    const editor = getConsumer(good());
    await wait();
    expect(editor.baseElement.textContent)
      .toEqual(JSON.stringify(good()[0].result.data.InviteUser));
  });

  it('Rejects the promise if something goes wrong', async () => {
    const editor = getConsumer(bad());
    await wait();
    expect(editor.baseElement.textContent)
      .toEqual(JSON.stringify(bad()[0].error.message));
  });
});
