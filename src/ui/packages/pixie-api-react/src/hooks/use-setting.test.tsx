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
import { MockedProvider } from '@apollo/client/testing';
import { act, render, RenderResult } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { USER_QUERIES, DEFAULT_USER_SETTINGS } from '@pixie-labs/api';
import { useSetting } from './use-setting';
import { wait } from '../testing/utils';

describe('useSetting hook for persistent user settings', () => {
  const getMocks = (tourSeen?: boolean|undefined, onMutate?: () => void) => [
    {
      request: {
        query: USER_QUERIES.GET_ALL_USER_SETTINGS,
        variables: {},
      },
      result: { data: { userSettings: [{ key: 'tourSeen', value: tourSeen }] } },
    },
    {
      request: {
        query: USER_QUERIES.SAVE_USER_SETTING,
        variables: { key: 'tourSeen', value: JSON.stringify(tourSeen) },
      },
      result: () => {
        onMutate?.();
        return { data: { userSettings: [{ key: 'tourSeen', value: tourSeen }] } };
      },
    },
  ];

  const Consumer = () => {
    const [tourSeen, setTourSeen] = useSetting('tourSeen');
    return <button type='button' onClick={() => setTourSeen(true)}>{JSON.stringify(tourSeen)}</button>;
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

  it('provides default values while loading', () => {
    const consumer = getConsumer(getMocks());
    expect(consumer.baseElement.textContent).toBe(String(DEFAULT_USER_SETTINGS.tourSeen));
  });

  it('provides default values when an error has occurred in reading stored values', () => {
    const errMocks = [{
      request: { query: USER_QUERIES.GET_ALL_USER_SETTINGS },
      error: 'Something went up in a ball of glorious fire, and took your request with it. Sorry about that.',
    }];
    const consumer = getConsumer(errMocks);
    expect(consumer.baseElement.textContent).toBe(String(DEFAULT_USER_SETTINGS.tourSeen));
  });

  it('recalls stored values', async () => {
    const consumer = getConsumer(getMocks(true));
    await wait();
    expect(consumer.baseElement.textContent).toBe('true');
  });

  it('asks Apollo to store updated values', async (done) => {
    await act(async () => {
      const consumer = getConsumer(getMocks(true, done));
      userEvent.click(consumer.baseElement.querySelector('button'));
      await wait();
    });
    // The "done" function is called above, as the second argument to getMocks.
  });
});
