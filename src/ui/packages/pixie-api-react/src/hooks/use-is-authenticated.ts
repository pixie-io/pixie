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

import * as React from 'react';
import { PixieAPIContext } from 'api-context';

/**
 * Checks if the current session has a valid bearer token to use Pixie's API, by requesting the `authorized` endpoint.
 * `authorized` responds with no body, using the HTTP status code to answer. As simple as it gets.
 */
export function useIsAuthenticated(): { loading: boolean; authenticated: boolean; error?: any } {
  const [promise, setPromise] = React.useState<Promise<boolean>>(null);
  // Using an object instead of separate variables because using multiple setState does NOT batch if it happens outside
  // of React's scope (like resolved promises or Observable subscriptions). To make it atomic, have to use ONE setState.
  const [{ loading, authenticated, error }, setState] = React.useState({
    loading: true, authenticated: false, error: undefined,
  });

  const client = React.useContext(PixieAPIContext);
  React.useEffect(() => {
    if (client) {
      setPromise(client.isAuthenticated());
    } else {
      throw new Error('useIsAuthenticated requires your component to be within a PixieAPIContextProvider to work!');
    }
  }, [setPromise, client]);

  React.useEffect(() => {
    if (!promise) return;
    setState({ loading: true, authenticated, error: undefined });
    promise.then((isAuthenticated) => {
      setState({ loading: false, authenticated: isAuthenticated, error: undefined });
    }).catch((e) => {
      setState({ loading: false, authenticated: false, error: e });
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [promise]);

  return { authenticated, loading, error };
}
