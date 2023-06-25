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

import UserContext, { User } from 'app/common/user-context';
import { WithChildren } from 'app/utils/react-boilerplate';

export const USER_CONTEXT_DEFAULTS: { user: User } = {
  user: {
    email: '',
    orgName: '',
  },
};

export const MockUserContextProvider: React.FC<WithChildren> = ({ children }) => (
  <UserContext.Provider value={USER_CONTEXT_DEFAULTS}>
    {children}
  </UserContext.Provider>
);
