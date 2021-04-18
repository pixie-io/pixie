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
import { MockedProvider } from '@apollo/client/testing';
import { act, render, RenderResult } from '@testing-library/react';
import { ApolloError } from '@apollo/client';
import { useUserInfo } from './use-user-info';
import { wait } from '../testing/utils';

describe('useUserInfo hook for user profile data', () => {
  const good = [{
    request: {
      query: USER_QUERIES.GET_USER_INFO,
    },
    result: {
      data: {
        user: {
          id: 'jdoe',
          email: 'jdoe@example.com',
          name: 'Jamie Doe',
          picture: '',
          orgName: 'ExampleCorp',
        },
      },
    },
  }];

  const bad = [{
    request: { query: USER_QUERIES.GET_USER_INFO },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  const Consumer = () => {
    const [user, loading, error] = useUserInfo();
    return <>{ JSON.stringify({ user, loading, error }) }</>;
  };

  const getConsumer = (mocks) => {
    let consumer: RenderResult;
    act(() => {
      consumer = render(
        <MockedProvider mocks={mocks} addTypename={false}>
          <Consumer />
        </MockedProvider>,
      );
    });
    return consumer;
  };

  it('represents loading state', () => {
    const consumer = getConsumer(good);
    const out = JSON.parse(consumer.baseElement.textContent);
    expect(out.loading).toBe(true);
  });

  it('retrieves correct data', async () => {
    const consumer = getConsumer(good);
    await wait();
    const out = JSON.parse(consumer.baseElement.textContent);
    expect(out.user).toEqual(good[0].result.data.user);
    expect(out.loading).toBe(false);
    expect(out.error).toBeFalsy();
  });

  it('presents errors', async () => {
    const consumer = getConsumer(bad);
    await wait();
    const out = JSON.parse(consumer.baseElement.textContent);
    expect(out.user).toBeFalsy();
    expect(out.loading).toBe(false);
    expect(out.error).toBeTruthy();
  });
});
