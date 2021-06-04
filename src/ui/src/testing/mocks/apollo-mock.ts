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
