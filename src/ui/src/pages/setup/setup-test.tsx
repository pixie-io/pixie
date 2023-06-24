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

import { ThemeProvider } from '@mui/material/styles';
import { act, render, waitFor } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';

import { DARK_THEME } from 'app/components';
import { SetupView } from 'app/pages/setup/setup';
import { MockPixieAPIContextProvider } from 'app/testing/mocks/api-context-mock';
import { MockClusterContextProvider } from 'app/testing/mocks/cluster-context-mock';
import { MockOrgContextProvider } from 'app/testing/mocks/org-context-mock';
import { MockSidebarContextProvider } from 'app/testing/mocks/sidebar-context-mock';
import { MockUserContextProvider } from 'app/testing/mocks/user-context-mock';

describe('setup page', () => {
  it('renders', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <ThemeProvider theme={DARK_THEME}>
          <MockPixieAPIContextProvider>
            <MockOrgContextProvider>
              <MockUserContextProvider>
                <MockClusterContextProvider>
                  <MockSidebarContextProvider>
                    <MemoryRouter initialEntries={['/setup-route?redirect_uri=%2Fnext%2Fpage']}>
                      <SetupView />
                    </MemoryRouter>
                  </MockSidebarContextProvider>
                </MockClusterContextProvider>
              </MockUserContextProvider>
            </MockOrgContextProvider>
          </MockPixieAPIContextProvider>
        </ThemeProvider>,
      ));
    });
    await waitFor(() => {});
    expect(container).toMatchSnapshot();
  });
});
