import * as React from 'react';
import { render, RenderResult, act } from '@testing-library/react';
import fetch from 'cross-fetch';
import { useIsAuthenticated } from './use-is-authenticated';
// noinspection ES6PreferShortImport
import { MockPixieAPIContextProvider, wait } from '../testing';

jest.mock('cross-fetch', () => ({
  default: jest.fn(),
}));

describe('useIsAuthenticated hook to check if the API can be accessed', () => {
  const Consumer: React.FC = () => {
    const content = useIsAuthenticated();
    const str = JSON.stringify(content);
    return <>{ str }</>;
  };

  const getConsumer = () => {
    let consumer;
    act(() => {
      consumer = render(
        <MockPixieAPIContextProvider>
          <Consumer />
        </MockPixieAPIContextProvider>,
      );
    });
    return consumer;
  };

  const getContent = (consumer: RenderResult) => {
    try {
      return JSON.parse(consumer.baseElement.textContent) as ReturnType<typeof useIsAuthenticated>;
    } catch {
      throw new Error(`Failed to parse as JSON: \`${String(consumer.baseElement.textContent)}\``);
    }
  };

  const mockFetchResponse = (response, reject = false) => {
    (fetch as jest.Mock).mockImplementation(
      () => (reject ? Promise.reject(response) : Promise.resolve(response)),
    );
  };

  it('treats HTTP 200 to mean successful authentication', async () => {
    mockFetchResponse({ status: 200 } as Response);
    const consumer = getConsumer();
    await wait();
    expect(getContent(consumer)).toStrictEqual({
      authenticated: true,
      loading: false,
    });
  });

  it('treats any other HTTP status as not authenticated', async () => {
    mockFetchResponse({ status: 500 } as Response);
    const consumer = getConsumer();
    await wait();
    expect(getContent(consumer)).toStrictEqual({
      authenticated: false,
      loading: false,
    });
  });

  it('treats a failed request as an error', async () => {
    mockFetchResponse('Ah, bugger.', true);
    const consumer = getConsumer();
    await wait();
    expect(getContent(consumer)).toStrictEqual({
      authenticated: false,
      loading: false,
      error: 'Ah, bugger.',
    });
  });
});
