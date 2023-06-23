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
import { BrowserRouter } from 'react-router-dom';

import { DARK_THEME } from 'app/components';
import { MockPixieAPIContextProvider } from 'app/testing/mocks/api-context-mock';

import { CLUSTER_BY_NAME_GQL, CLUSTER_NAV_GQL, ClusterDetails } from './cluster-details';

/* eslint-disable react-memo/require-memo, react-memo/require-usememo */

const mocks: MockedResponse[] = [
  {
    request: {
      query: CLUSTER_NAV_GQL,
      variables: {},
    },
    result: {
      data: {
        clusters: [
          { id: 'foo:id', clusterName: 'foo:name', prettyClusterName: 'foo:prettyName', status: 'healthy' },
          { id: 'bar:id', clusterName: 'bar:name', prettyClusterName: 'bar:prettyName', status: 'healthy' },
        ],
      },
    },
  },
  {
    request: {
      query: CLUSTER_BY_NAME_GQL,
      variables: { name: 'foo:name' },
    },
    result: {
      data: {
        clusterByName: {
          id: 'foo:id',
          clusterName: 'foo:name',
          clusterVersion: '0.0.0',
          operatorVersion: '0.0.0',
          vizierVersion: '0.0.0-dev',
          prettyClusterName: 'foo:prettyName',
          clusterUID: 'a:b:c:d',
          status: 'healthy',
          statusMessage: '',
          lastHeartbeatMs: 0,
          numNodes: 1,
          numInstrumentedNodes: 1,
          controlPlanePodStatuses: [],
          unhealthyDataPlanePodStatuses: [],
        },
      },
    },
  },
  {
    request: {
      query: CLUSTER_BY_NAME_GQL,
      variables: { name: 'bar:name' },
    },
    result: {
      data: {
        clusterByName: {
          id: 'bar:id',
          clusterName: 'bar:name',
          clusterVersion: '0.0.0',
          operatorVersion: '0.0.0',
          vizierVersion: '0.0.0-dev',
          prettyClusterName: 'bar:prettyName',
          clusterUID: 'b:c:d:a',
          status: 'healthy',
          statusMessage: '',
          lastHeartbeatMs: 0,
          numNodes: 1,
          numInstrumentedNodes: 1,
          controlPlanePodStatuses: [],
          unhealthyDataPlanePodStatuses: [],
        },
      },
    },
  },
];

const Tester = ({ clusterName }) => (
  <ThemeProvider theme={DARK_THEME}>
    <BrowserRouter>
      <MockPixieAPIContextProvider mocks={mocks}>
        <ClusterDetails name={clusterName} />
      </MockPixieAPIContextProvider>
    </BrowserRouter>
  </ThemeProvider>
);

describe('<ClusterDetails />', () => {
  it('renders the details of the given cluster', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester clusterName='foo:name' />,
      ));
    });
    await waitFor(() => {});
    const el = await within(container).findByText('foo:name');
    expect(el.tagName).toBe('TD');
  });

  it('has breadcrumbs for each cluster it finds', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester clusterName='foo:name' />,
      ));
    });
    await waitFor(() => {});
    const opener = await within(container).findByText('foo:prettyName');
    expect(opener).toBeDefined();
    await act(async () => {
      fireEvent.click(opener);
    });
    await waitFor(() => {});

    // Have to do this in order, since the selected foo:prettyName text already exists before the dropdown opens
    expect(await screen.findAllByText('bar:prettyName')).toHaveLength(1);
    expect(await screen.findAllByText('foo:prettyName')).toHaveLength(2);
  });

  it('switches clusters in its dropdown', async () => {
    let container: HTMLElement;
    await act(async () => {
      ({ container } = render(
        <Tester clusterName='foo:name' />,
      ));
    });
    await waitFor(() => {});
    const opener = await within(container).findByText('foo:prettyName');
    expect(opener).toBeDefined();
    await act(async () => {
      fireEvent.click(opener);
    });
    await waitFor(() => {});

    const pushSpy = jest.spyOn(window.history, 'pushState');

    // Click the other cluster in the dropdown
    await act(async () => {
      fireEvent.click(await screen.findByText('bar:prettyName'));
    });

    // ... and, since the routing logic is in another component, just check that the history push happened.
    // It results in changing the `name` prop of ClusterDetails, which is already tested above.
    expect(pushSpy).toHaveBeenCalledWith(expect.any(Object), null, '/admin/clusters/bar:name');
  });
});
