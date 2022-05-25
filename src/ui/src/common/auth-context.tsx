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

import { WithChildren } from 'app/utils/react-boilerplate';

export interface AuthContextProps {
  authToken: string;
  setAuthToken: (token: string) => void;
}

export const AuthContext = React.createContext<AuthContextProps>(null);
AuthContext.displayName = 'AuthContext';

export const AuthContextProvider = React.memo<WithChildren>(({ children }) => {
  const [authToken, setAuthToken] = React.useState<string>('');

  return (
    <AuthContext.Provider value={{ authToken, setAuthToken }}>
      {children}
    </AuthContext.Provider>
  );
});
AuthContextProvider.displayName = 'AuthContextProvider';
