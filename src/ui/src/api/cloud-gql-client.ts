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
  ServerParseError,
  gql,
} from '@apollo/client/core';
import { setContext } from '@apollo/client/link/context';
import { onError } from '@apollo/client/link/error';
import { CachePersistor } from 'apollo3-cache-persist';
import fetch from 'cross-fetch';

import { isPixieEmbedded } from 'app/common/embed-context';
import { GetCSRFCookie } from 'app/pages/auth/utils';

import { PixieAPIClientOptions } from './api-options';

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
const loginRedirectLink = (on401: (errorMessage?: string) => void) => onError(({ networkError, operation }) => {
  const isEmbed = isPixieEmbedded();
  const isLogin = window.location.pathname.endsWith('/login');
  const isCacheOnly = operation.operationName.endsWith('Cache');
  if (isEmbed || isLogin || isCacheOnly) {
    return;
  }

  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    on401((networkError as ServerParseError).bodyText?.trim() ?? networkError.message);
  }
});

/**
 * Uses localStorage if it's available; uses an in-memory Map otherwise.
 * NodeJS doesn't implement it.
 * Browsers sometimes block access (iframes, incognito, etc).
 */
const localStorageShim = (() => {
  let useLocalStorage = true;
  try {
    globalThis.localStorage.getItem('checkAccess');
  } catch (e) {
    useLocalStorage = false;
  }

  return useLocalStorage ? globalThis.localStorage : (() => {
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
})();

export class CloudClient {
  graphQL: ApolloClient<NormalizedCacheObject>;

  private readonly cache: InMemoryCache;

  private persistor: CachePersistor<NormalizedCacheObject>;

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
        ApolloLink.split(
          (operation) => operation.getContext().connType === 'unauthenticated',
          createHttpLink({ uri: `${opts.uri}/unauthenticated/graphql`, fetch }),
          createHttpLink({ uri: `${opts.uri}/graphql`, fetch }),
        ),
      ]),
    });

    // Cache persistence is isolated per-user, and we use their ID to do that.
    // The user's ID comes from a query too, so we use InMemoryCache at first.
    // Persistence catches up (merging InMemoryCache on top) when ID is ready.
    let userId: string;
    this.cache.watch({
      optimistic: true,
      query: gql`
        query getUserIdFromCache {
          user {
            id
          }
        }
      `,
      callback: ({ result }) => {
        // Don't persist until we know which user we're caching for.
        if (!result.user?.id) return;

        // If nothing changed, don't reset persistence.
        if (userId && userId === result.user.id) return;

        // The user ID shouldn't be changing without a logout or refresh.
        // If it does, that's a new feature that we didn't account for, so throw.
        if (userId && userId !== result.user.id) {
          throw new Error(
            `Cache key is set to "${userId}", but user ID changed to "${result.user.id}"`,
          );
        }

        userId = result.user.id;
        this.setupPersistence(userId);
      },
    });
  }

  /**
   * Cache persistence isn't enabled right away, as it's isolated per-user.
   * To get the user ID, we need to start with InMemoryCache and query upstream.
   * Once we have the ID, we restore their cache (if present), merge the
   * in-memory cache onto it, and resume normal persistence - all silently.
   *
   * @param userId User whose cache should be used.
   */
  private async setupPersistence(userId: string) {
    const cacheKey = `apollo-cache-${userId}`;

    // Grab contents of in-memory cache before clobbering
    const memory = this.cache.extract();

    this.persistor = new CachePersistor({
      key: cacheKey,
      cache: this.cache,
      storage: localStorageShim,
    });

    // Pause auto-save until we're done moving data around.
    this.persistor.pause();

    // Clobber in-memory cache with what was persisted (if it existed),
    // Then grab those contents to compare against what was in-memory before.
    await this.persistor.restore();
    const stored = this.cache.extract();

    // Overwrite objects in the stored cache with conflicting ones from memory.
    // Should technically be a deep merge; clobbering is good enough though.
    const merged: NormalizedCacheObject = {
      ...stored,
      ...memory,
    };
    if (memory.ROOT_QUERY || stored.ROOT_QUERY) {
      merged.ROOT_QUERY = {
        ...(stored.ROOT_QUERY ?? {}),
        ...(memory.ROOT_QUERY ?? {}),
      };
    }
    if (memory.ROOT_MUTATION || stored.ROOT_MUTATION) {
      merged.ROOT_MUTATION = {
        ...(stored.ROOT_MUTATION ?? {}),
        ...(memory.ROOT_MUTATION ?? {}),
      };
    }

    // As before, restoring a cache purges what's there first.
    this.cache.restore(merged);

    // Running garbage collection removes cached entries that aren't referenced anywhere in ROOT_QUERY.
    // Example: if the user joins an org, this will remove the entry about their previous (null) org.
    // This also makes sure that anything overridden from `stored` by `memory` gets cleaned up.
    this.cache.gc();

    // Done manipulating data; let it flush to storage.
    this.persistor.resume();
  }

  async purgeCache(): Promise<void> {
    this.persistor?.pause();
    await this.persistor?.purge();
    await this.graphQL.clearStore();
    this.persistor?.resume();
  }
}
