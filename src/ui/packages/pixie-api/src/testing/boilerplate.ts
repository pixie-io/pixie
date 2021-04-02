import type { ApolloClient } from '@apollo/client/core';

import * as apolloDependency from '@apollo/client/core';

// Imported so it can be mocked
import 'apollo3-cache-persist';

/**
 * Mocks ApolloClient's core; returns Jest mocks for the query and mutate methods.
 * Note: this uses beforeEach() to create the spies. It must be used outside of an individual test as such.
 * If you add mock implementation to the returned methods, be sure to reset it between tests!
 */
export function mockApolloClient(): {
  query: jest.MockedFunction<InstanceType<typeof ApolloClient>['query']>;
  watchQuery: jest.MockedFunction<InstanceType<typeof ApolloClient>['watchQuery']>;
  mutate: jest.MockedFunction<InstanceType<typeof ApolloClient>['mutate']>;
} {
  // GQL functionality runs through Apollo, which does provide its own mock features.
  // However, those mocks are exposed in a React context.
  // As this is a framework-independent library, it can't reasonably use that. Thus, Jest mock.
  const query = jest.fn();
  const watchQuery = jest.fn();
  const mutate = jest.fn();

  const mockApollo = {
    ApolloClient: () => ({ query, watchQuery, mutate }),
    InMemoryCache: jest.fn(),
    ApolloLink: {
      from: jest.fn(),
    },
    createHttpLink: jest.fn(),
  };

  beforeEach(() => {
    spyOn(apolloDependency, 'ApolloClient').and.returnValue(mockApollo.ApolloClient());
    spyOn(apolloDependency, 'InMemoryCache').and.returnValue(mockApollo.InMemoryCache);
    spyOn(apolloDependency, 'ApolloLink').and.returnValue(mockApollo.ApolloLink);
    spyOn(apolloDependency, 'createHttpLink').and.returnValue(mockApollo.createHttpLink);
  });

  return { query, watchQuery, mutate };
}
