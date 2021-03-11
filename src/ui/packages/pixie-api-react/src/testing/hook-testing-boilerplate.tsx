import * as React from 'react';
import { MockedResponse, MockedProvider } from '@apollo/client/testing';
import {
  act, cleanup, render, RenderResult,
} from '@testing-library/react';
import { ApolloError, DefaultOptions } from '@apollo/client';
import { wait } from './utils';

/**
 * Use these settings in a <MockedProvider /> to ensure Apollo actually uses mock responses rather than the persistent
 * cache (which can interfere in unpredictable ways). Each query can override this; this is merely changing defaults.
 */
export const MOCKED_PROVIDER_DEFAULT_SETTINGS: DefaultOptions = {
  query: { fetchPolicy: 'network-only' },
  watchQuery: { fetchPolicy: 'network-only' },
  mutate: { fetchPolicy: 'no-cache' },
};

interface BasicHookTestOptions {
  happyMocks: ReadonlyArray<MockedResponse>;
  sadMocks: ReadonlyArray<MockedResponse>;
  /**
   * The function that actually invokes the hook under test.
   * Returns the UNWRAPPED data, loading state, and Apollo error (if any).
   */
  useHookUnderTest: () => ({ payload: any; loading: boolean; error?: ApolloError });
  /**
   * When checking that data and error information are both correct, this test needs to extract the appropriate
   * payload info from the mocks for comparison. Provide this function to tell it how to do that.
   * @param mock
   */
  getPayloadFromMock: (mock: MockedResponse) => any;
}

export function defineConsumer(hookRunner: BasicHookTestOptions['useHookUnderTest']) {
  const Consumer: React.FC = () => {
    const payload = hookRunner();
    return <>{ JSON.stringify(payload) }</>;
  };
  return Consumer;
}

/**
 * Every hook needs to test that it presents loading state and any errors, plus some basic happy path data.
 * Those tests have a lot of boilerplate; this function builds them for you.
 * @param opts
 */
export function itPassesBasicHookTests(opts: BasicHookTestOptions) {
  describe('Common Pixie React hook tests', () => {
    afterEach(cleanup);

    const Consumer = defineConsumer(opts.useHookUnderTest);
    const getConsumer = (mocks: ReadonlyArray<MockedResponse>) => {
      let consumer: RenderResult;
      act(() => {
        consumer = render(
          <MockedProvider mocks={mocks} addTypename={false} defaultOptions={MOCKED_PROVIDER_DEFAULT_SETTINGS}>
            <Consumer />
          </MockedProvider>,
        );
      });
      // noinspection JSUnusedAssignment
      return consumer;
    };

    it('represents the loading state', () => {
      const consumer = getConsumer(opts.happyMocks);
      const out = JSON.parse(consumer.baseElement.textContent);
      expect(out.loading).toBe(true);
      expect(out.error).toBeFalsy();
    });

    it('retrieves expected data', async () => {
      const consumer = getConsumer(opts.happyMocks);
      await wait();
      const out = JSON.parse(consumer.baseElement.textContent);
      expect(out.payload).toEqual(opts.getPayloadFromMock(opts.happyMocks[0]));
      expect(out.loading).toBe(false);
      expect(out.error).toBeFalsy();
    });

    it('presents errors', async () => {
      const consumer = getConsumer(opts.sadMocks);
      await wait();
      const out = JSON.parse(consumer.baseElement.textContent);
      expect(out.payload).toBeFalsy();
      expect(out.loading).toBe(false);
      expect(out.error).toBeTruthy();
    });
  });
}
