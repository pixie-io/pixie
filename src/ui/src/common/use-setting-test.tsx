import * as React from 'react';
import { mount } from 'enzyme';
import { MockedProvider } from '@apollo/react-testing';
import { act } from 'react-dom/test-utils';
import { USER_QUERIES, DEFAULT_USER_SETTINGS } from 'pixie-api';
import { useSetting } from './use-setting';

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

  const getConsumer = (mocks) => mount(
    <MockedProvider mocks={mocks} addTypename={false}>
      <Consumer />
    </MockedProvider>,
  );

  it('provides default values while loading', () => {
    const consumer = getConsumer(getMocks());
    expect(consumer.render().text()).toBe(String(DEFAULT_USER_SETTINGS.tourSeen));
    consumer.unmount();
  });

  it('provides default values when an error has occurred in reading stored values', () => {
    const errMocks = [{
      request: { query: USER_QUERIES.GET_ALL_USER_SETTINGS },
      error: 'Something went up in a ball of glorious fire, and took your request with it. Sorry about that.',
    }];
    const consumer = getConsumer(errMocks);
    expect(consumer.render().text()).toBe(String(DEFAULT_USER_SETTINGS.tourSeen));
    consumer.unmount();
  });

  it('recalls stored values', async (done) => {
    const consumer = getConsumer(getMocks(true));
    await act(async () => {
      consumer.update();
      await new Promise((resolve) => setTimeout(resolve));
    });
    expect(consumer.render().text()).toBe('true');
    consumer.unmount();
    done();
  });

  it('asks Apollo to store updated values', async (done) => {
    const consumer = getConsumer(getMocks(true, done));
    await act(async () => {
      consumer.find('button').simulate('click');
      consumer.update();
      await new Promise((resolve) => setTimeout(resolve));
    });
    consumer.unmount();
  }, 100);
});
