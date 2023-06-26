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

import { MockedResponse } from '@apollo/client/testing';
import { ThemeProvider } from '@mui/material/styles';
import { act, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import Axios from 'axios';
import { MemoryRouter, Route } from 'react-router-dom';

import { DARK_THEME } from 'app/components';
import { CREATE_ORG_GQL, SetupRedirect, SetupView } from 'app/pages/setup/setup';
import { MockPixieAPIContextProvider } from 'app/testing/mocks/api-context-mock';
import { MockClusterContextProvider } from 'app/testing/mocks/cluster-context-mock';
import { MockOrgContextProvider } from 'app/testing/mocks/org-context-mock';
import { MockSidebarContextProvider } from 'app/testing/mocks/sidebar-context-mock';
import { MockUserContextProvider } from 'app/testing/mocks/user-context-mock';

const mocks: MockedResponse[] = [
  {
    request: {
      query: CREATE_ORG_GQL,
      variables: { orgName: 'validorg' },
    },
    result: {
      data: {
        CreateOrg: {
          id: 'id-valid-org',
        },
      },
    },
  },
  {
    request: {
      query: CREATE_ORG_GQL,
      variables: { orgName: 'conflictingorg' },
    },
    error: new Error('Org already exists'),
  },
];

const Tester = () => (
  <ThemeProvider theme={DARK_THEME}>
    <MockPixieAPIContextProvider mocks={mocks}>
      <MockOrgContextProvider>
        <MockUserContextProvider>
          <MockClusterContextProvider>
            <MockSidebarContextProvider>
              <MemoryRouter initialEntries={['/setup/page?redirect_uri=%2Fnext%2Fpage']}>
                <Route path='/setup/page' component={SetupView} />
              </MemoryRouter>
            </MockSidebarContextProvider>
          </MockClusterContextProvider>
        </MockUserContextProvider>
      </MockOrgContextProvider>
    </MockPixieAPIContextProvider>
  </ThemeProvider>
);

let origLocation: Location;
beforeAll(() => {
  origLocation = window.location;
  delete global.window.location;
  Object.defineProperty(global.window, 'location', {
    value: new URL('http://localhost'),
    writable: true,
  });
});

afterAll(() => {
  global.window.location = origLocation;
});

describe('setup page', () => {
  it('renders', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester />,
      ));
    });
    await waitFor(() => {});
    expect(container).toMatchSnapshot();
  });

  it('validates org name', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester />,
      ));
    });
    await waitFor(() => {});
    const el = await within(container).getByRole('textbox');

    fireEvent.change(el, { target: { value: 'a' } });
    await screen.findByText('Name is too short');

    fireEvent.change(el, { target: { value: 'a'.repeat(51) } });
    await screen.findByText('Name is too long');

    fireEvent.change(el, { target: { value: 'testingorg.com' } });
    await screen.findByText('Name must not contain special characters (ex. ./\\$@)');

    fireEvent.change(el, { target: { value: 'validorg' } });
    await waitFor(() => {});
    let errorMsg = screen.queryByText('Name is too short');
    expect(errorMsg).toBeNull();
    errorMsg = screen.queryByText('Name is too long');
    expect(errorMsg).toBeNull();
    errorMsg = screen.queryByText('Name must not contain special characters (ex. ./\\$@)');
    expect(errorMsg).toBeNull();
  });

  it('creats org and redirects', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester />,
      ));
    });

    jest.mock('axios');
    Axios.post = jest.fn().mockResolvedValue({});

    const hrefSpyFn = jest.fn();
    Object.defineProperty(window.location, 'href', {
      set: hrefSpyFn,
      enumerable: true,
      configurable: true,
    });

    await waitFor(() => {});
    const el = await within(container).getByRole('textbox');
    fireEvent.change(el, { target: { value: 'validorg' } });
    const button = await within(container).findByText('Create');
    await act(async () => {
      fireEvent.click(button);
    });

    await expect(hrefSpyFn).toBeCalledWith('/next/page');
    delete window.location.href;
  });

  it('renders error on conflicting org name', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester />,
      ));
    });
    await waitFor(() => {});
    const el = await within(container).getByRole('textbox');
    fireEvent.change(el, { target: { value: 'conflictingorg' } });
    const form = await within(container).getByRole('form');
    await act(async () => {
      fireEvent.submit(form);
    });

    const errorMsg = screen.queryByText('Org already exists');
    expect(errorMsg).not.toBeNull();
  });

  it('immediately redirects', async () => {
    await act(async () => {
      render(
        <MemoryRouter initialEntries={['/setup/redirect?redirect_uri=%2Fnext%2Fpage']}>
          <Route path='/setup/redirect' component={SetupRedirect} />
          <Route path='/next/page' render={() => (<div>Redirected to next page</div>)} />
        </MemoryRouter>,
      );
    });
    await screen.findByText('Redirected to next page');
  });
});
