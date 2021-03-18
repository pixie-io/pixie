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
