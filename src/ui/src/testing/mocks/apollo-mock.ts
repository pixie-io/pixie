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

// Jest can only mock something that gets imported, even if that import is unused. Thus, side effect imports.
import '@apollo/client/core';
import 'apollo3-cache-persist';

/**
 * Mocks ApolloClient's core; returns Jest mocks for the query and mutate methods.
 * Note: this uses beforeEach() to create the spies. It must be used outside of an individual test as such.
 * If you add mock implementation to the returned methods, be sure to reset it between tests!
 */
export function mockApolloClient(): void {
  jest.mock('@apollo/client/core', () => ({
    ApolloClient: () => ({ query: jest.fn(), watchQuery: jest.fn(), mutate: jest.fn() }),
    InMemoryCache: jest.fn(),
    ApolloLink: {
      from: jest.fn(),
    },
    createHttpLink: jest.fn(),
  }));
}
