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

import { ApolloProvider } from '@apollo/client/react';

import { AuthContext } from 'app/common/auth-context';
import { SetStateFunc } from 'app/context/common';
import { WithChildren } from 'app/utils/react-boilerplate';

import { PixieAPIClient, PixieAPIClientAbstract } from './api-client';
import { PixieAPIManager } from './api-manager';
import { PixieAPIClientOptions } from './api-options';

export const PixieAPIContext = React.createContext<PixieAPIClientAbstract>(null);
PixieAPIContext.displayName = 'PixieAPIContext';

export type PixieAPIContextProviderProps = WithChildren<PixieAPIClientOptions>;

declare global {
  interface Window {
    setApiContextUpdatesFromOutsideReact?: SetStateFunc<number>;
  }
}

export const PixieAPIContextProvider: React.FC<PixieAPIContextProviderProps> = React.memo(({ children, ...opts }) => {
  const { authToken } = React.useContext(AuthContext);

  // PixieAPIManager exists outside of React's scope. If it replaces its instance, we'll never know.
  // By putting a setState function somewhere that PixieAPIManager can reach, we can force the update from there.
  // This is not a typical or conventional thing to need to do, so please don't do this anywhere else.
  const [updateFromOutsideReact, setUpdateFromOutsideReact] = React.useState(0);
  React.useEffect(() => {
    // Technically this means PixieAPIContextProvider has to be singleton - and it already is, in practice.
    if (!window.setApiContextUpdatesFromOutsideReact) {
      window.setApiContextUpdatesFromOutsideReact = setUpdateFromOutsideReact;
    }
    return () => {
      delete window.setApiContextUpdatesFromOutsideReact;
    };
  }, []);
  React.useEffect(() => { /* Just need to update this context, nothing more */ }, [updateFromOutsideReact]);

  // PixieAPIManager already reinitializes the API client when options change, making this context just a wrapper.
  React.useEffect(() => { PixieAPIManager.uri = opts.uri; }, [opts.uri]);
  React.useEffect(() => { PixieAPIManager.authToken = authToken; }, [authToken]);
  React.useEffect(() => { PixieAPIManager.onUnauthorized = opts.onUnauthorized; }, [opts.onUnauthorized]);

  const instance = PixieAPIManager.instance as PixieAPIClient;
  const gqlClient = instance.getCloudClient().graphQL;
  React.useEffect(() => { /* Similar to the above trick, must re-render to update ApolloProvider */ }, [gqlClient]);

  return !instance ? null : (
    <PixieAPIContext.Provider value={instance}>
      <ApolloProvider client={instance.getCloudClient().graphQL}>
        { children }
      </ApolloProvider>
    </PixieAPIContext.Provider>
  );
});
PixieAPIContextProvider.displayName = 'PixieAPIContextProvider';
