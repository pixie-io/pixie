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
import { waitFor, render } from '@testing-library/react';

import { DARK_THEME } from 'app/components';
import { MockClusterContextProvider } from 'app/testing/mocks/cluster-context-mock';
import { MockLiveRouteContextProvider } from 'app/testing/mocks/live-routing-mock';
import { MockScriptContextProvider } from 'app/testing/mocks/script-context-mock';
import { MockScriptsContextProvider } from 'app/testing/mocks/scripts-context-mock';

import { LiveViewBreadcrumbs } from './breadcrumbs';

describe('Live view breadcrumbs', () => {
  it('renders', async () => {
    render(
      <ThemeProvider theme={DARK_THEME}>
        <MockClusterContextProvider>
          <MockScriptsContextProvider>
            <MockLiveRouteContextProvider>
              <MockScriptContextProvider>
                <LiveViewBreadcrumbs />
              </MockScriptContextProvider>
            </MockLiveRouteContextProvider>
          </MockScriptsContextProvider>
        </MockClusterContextProvider>
      </ThemeProvider>,
    );
    await waitFor(() => {}); // So that useEffect can settle down (otherwise we get "use act()" complaints)
  });
});
