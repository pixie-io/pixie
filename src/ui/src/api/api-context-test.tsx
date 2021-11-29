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

import { screen, render } from '@testing-library/react';

import { AuthContextProvider } from 'app/common/auth-context';

import { PixieAPIContextProvider } from './api-context';

describe('Pixie API React Context', () => {
  it('renders once the context is ready', async () => {
    render(<AuthContextProvider>
        <PixieAPIContextProvider apiKey=''>Hello</PixieAPIContextProvider>
      </AuthContextProvider>);
    await screen.findByText('Hello');
  });
});
