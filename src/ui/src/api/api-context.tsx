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
import { WithChildren } from 'app/utils/react-boilerplate';

import { PixieAPIClient, PixieAPIClientAbstract } from './api-client';
import { PixieAPIManager } from './api-manager';
import { PixieAPIClientOptions } from './api-options';


export const PixieAPIContext = React.createContext<PixieAPIClientAbstract>(null);
PixieAPIContext.displayName = 'PixieAPIContext';

export type PixieAPIContextProviderProps = WithChildren<PixieAPIClientOptions>;

export const PixieAPIContextProvider: React.FC<PixieAPIContextProviderProps> = React.memo(({ children, ...opts }) => {
  const { authToken } = React.useContext(AuthContext);

  // PixieAPIManager already reinitializes the API client when options change, making this context just a wrapper.
  React.useEffect(() => { PixieAPIManager.uri = opts.uri; }, [opts.uri]);
  React.useEffect(() => { PixieAPIManager.authToken = authToken; }, [authToken]);
  React.useEffect(() => { PixieAPIManager.onUnauthorized = opts.onUnauthorized; }, [opts.onUnauthorized]);

  const instance = PixieAPIManager.instance as PixieAPIClient;

  return !instance ? null : (
    <PixieAPIContext.Provider value={instance}>
      <ApolloProvider client={instance.getCloudClient().graphQL}>
        { children }
      </ApolloProvider>
    </PixieAPIContext.Provider>
  );
});
PixieAPIContextProvider.displayName = 'PixieAPIContextProvider';
