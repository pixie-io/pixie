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

import {
  ApolloClient,
  InMemoryCache,
  NormalizedCacheObject,
  ApolloLink,
  createHttpLink,
  ServerError,
  gql,
} from '@apollo/client/core';
import { setContext } from '@apollo/client/link/context';
import { onError } from '@apollo/client/link/error';
import { persistCache } from 'apollo3-cache-persist';
import fetch from 'cross-fetch';
import { PixieAPIClientOptions } from './api-options';
import { GetCSRFCookie } from '../pages/auth/utils';

// Apollo link that adds cookies in the request.
const makeCloudAuthLink = (opts: PixieAPIClientOptions) => setContext((_, { headers }) => ({
  headers: {
    ...headers,
    'x-csrf': GetCSRFCookie(),
    ...(opts.authToken ? { authorization: `Bearer ${opts.authToken}`, 'X-Use-Bearer': true } : {}),
    // NOTE: apiKey is required in the interface because every consumer EXCEPT Pixie's web UI must provide it.
    // Pixie's web UI provides the empty string to indicate that it's using credentials instead.
    // If any other consumer tries to do the same thing in a browser, CORS will block the request on the API side.
    ...(opts.apiKey ? { 'pixie-api-key': opts.apiKey } : { withCredentials: true }),
  },
}));

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = (on401: (errorMessage?: string) => void) => onError(({ networkError }) => {
  if (window.location.pathname.startsWith('/embed')) {
    return;
  }

  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    on401(networkError.message);
  }
});

export interface ClusterConnection {
  ipAddress: string;
  token: string;
}

export interface GetClusterConnResults {
  clusterConnection: ClusterConnection;
}

export class CloudClient {
  graphQL: ApolloClient<NormalizedCacheObject>;

  private readonly persistPromise: Promise<void>;

  private readonly cache: InMemoryCache;

  private loaded = false;

  constructor(opts: PixieAPIClientOptions) {
    this.cache = new InMemoryCache({
      typePolicies: {
        AutocompleteResult: {
          keyFields: ['formattedInput'],
        },
      },
    });

    this.graphQL = new ApolloClient({
      connectToDevTools: globalThis.process?.env && process.env.NODE_ENV === 'development',
      cache: this.cache,
      link: ApolloLink.from([
        makeCloudAuthLink(opts),
        loginRedirectLink(opts.onUnauthorized ?? (() => {})),
        createHttpLink({ uri: `${opts.uri}/graphql`, fetch }),
      ]),
    });

    // On NodeJS, there is no localStorage. However, we still want to cache at runtime, so use a Map.
    // In a browser, we can use localStorage normally.
    let useLocalStorage = true;
    try {
      // Checks if localStorage is defined, and if localStorge is accessible. The latter
      // may not be possible if in an embedded environment.
      globalThis.localStorage.setItem('checkAccess', '');
    } catch (e) {
      useLocalStorage = false;
    }

    const storage = useLocalStorage ? globalThis.localStorage : (() => {
      const map = new Map<string, any>();
      return {
        getItem: (k: string) => map.get(k),
        setItem: (k: string, v: any) => {
          map.set(k, v);
        },
        removeItem: (k: string) => {
          map.delete(k);
        },
      };
    })();

    this.persistPromise = persistCache({
      cache: this.cache,
      storage,
    }).then(() => {
      this.loaded = true;
    });
  }

  async getGraphQLPersist(): Promise<CloudClient['graphQL']> {
    if (!this.loaded) {
      await this.persistPromise;
    }
    return this.graphQL;
  }

  /**
   * Implementation detail for forming a connection to a cluster for health check and script execution requests.
   */
  async getClusterConnection(id: string, noCache = false): Promise<ClusterConnection> {
    const { data } = await this.graphQL.query<GetClusterConnResults>({
      query: gql`
        query GetClusterConnection($id: ID!) {
          clusterConnection(id: $id) {
            ipAddress
            token
          }
        }
      `,
      variables: { id },
      fetchPolicy: noCache ? 'network-only' : 'cache-first',
    });
    return data.clusterConnection;
  }
}
