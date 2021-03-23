import { ApolloClient } from '@apollo/client/core';

/**
 * Mocks ApolloClient's core; returns Jest mocks for the query and mutate methods.
 * This method has side effects! It uses a beforeEach and an afterEach to create and reset the mocks.
 * This allows adding implementation to the mocks on a per-test or per-describe-block basis.
 */
export function mockApolloClient(): {
  query: InstanceType<typeof ApolloClient>['query'];
  mutate: InstanceType<typeof ApolloClient>['mutate'];
} {
  // GQL functionality runs through Apollo, which does provide its own mock features.
  // However, those mocks are exposed in a React context.
  // As this is a framework-independent library, it can't reasonably use that. Thus, Jest mock.
  const query = jest.fn();
  const mutate = jest.fn();
  beforeEach(() => {
    const apolloMock = {
      ApolloClient: () => ({ query, mutate }),
      InMemoryCache: jest.fn(),
      ApolloLink: {
        from: jest.fn(),
      },
      createHttpLink: jest.fn(),
    };
    jest.mock('@apollo/client', () => apolloMock);
    jest.mock('@apollo/client/core', () => apolloMock);
  });

  afterEach(() => {
    query.mockReset();
    mutate.mockReset();
  });

  return { query, mutate };
}
