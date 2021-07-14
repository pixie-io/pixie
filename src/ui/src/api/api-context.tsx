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
import { makeCancellable, silentlyCatchCancellation } from 'app/utils/cancellable-promise';
import { AuthContext } from 'app/common/auth-context';
import { PixieAPIClientAbstract, PixieAPIClient } from './api';
import { PixieAPIClientOptions } from './api-options';

export const PixieAPIContext = React.createContext<PixieAPIClientAbstract>(null);

export type PixieAPIContextProviderProps = PixieAPIClientOptions;

export const PixieAPIContextProvider: React.FC<PixieAPIContextProviderProps> = ({ children, ...opts }) => {
  const [pixieClient, setPixieClient] = React.useState<PixieAPIClient>(null);
  const { authToken } = React.useContext(AuthContext);

  React.useEffect(() => {
    const creator = makeCancellable(PixieAPIClient.create({ ...opts, authToken }));
    creator.then(setPixieClient).catch(silentlyCatchCancellation);
    return () => {
      creator.cancel();
      setPixieClient(null);
    };
    // Destructuring the options instead of checking directly because the identity of the object changes every render.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [opts.uri, opts.onUnauthorized, authToken]);

  return !pixieClient ? null : (
    <PixieAPIContext.Provider value={pixieClient}>
      <ApolloProvider client={pixieClient.getCloudClient().graphQL}>
        { children }
      </ApolloProvider>
    </PixieAPIContext.Provider>
  );
};
