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
import { act, fireEvent, render, screen, waitFor, within } from '@testing-library/react';

import { DARK_THEME } from 'app/components';
import { GQLPodStatus } from 'app/types/schema';

import { PixiePodsTab } from './cluster-details-pods';

/* eslint-disable react-memo/require-memo, react-memo/require-usememo */

const healthyCont = 'CONTAINER_STATE_RUNNING';

const mockControlPlanePods: GQLPodStatus[] = [
  {
    name: 'control-pod-foo',
    status: 'RUNNING',
    message: '',
    reason: '',
    createdAtMs: 0,
    restartCount: 0,
    events: [
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Something probably good happened' },
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Mundane, boring, typical events.' },
    ],
    containers: [
      { name: 'control-pod-foo-foo', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
      { name: 'control-pod-foo-bar', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
    ],
  },
  {
    name: 'control-pod-bar',
    status: 'FAILED',
    message: '',
    reason: '',
    createdAtMs: 0,
    restartCount: 0,
    events: [
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Something probably good happened' },
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Mundane, boring, typical events.' },
    ],
    containers: [],
  },
];

const mockDataPlanePods: GQLPodStatus[] = [
  {
    name: 'data-pod-foo',
    status: 'PENDING',
    message: 'Disconnected',
    reason: 'Because I, a unit test, said so. That is within my power. Fear me.',
    createdAtMs: 0,
    restartCount: 1_000,
    events: [
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Bad news' },
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Worse news' },
    ],
    containers: [
      { name: 'data-pod-foo-foo', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
      { name: 'data-pod-foo-bar', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
    ],
  },
  {
    name: 'data-pod-bar',
    status: 'UNKNOWN',
    message: 'Disconnected',
    reason: 'Because I, a unit test, said so. That is within my power. Fear me.',
    createdAtMs: 0,
    restartCount: 1_000,
    events: [
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Bad news' },
      { firstTimeMs: 0, lastTimeMs: 1, message: 'Worse news' },
    ],
    containers: [
      { name: 'data-pod-bar-foo', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
      { name: 'data-pod-bar-bar', message: '', reason: '', createdAtMs: 0, restartCount: 0, state: healthyCont },
    ],
  },
];

const Tester = ({ control, data }: { control: GQLPodStatus[], data: GQLPodStatus[] }) => (
  <ThemeProvider theme={DARK_THEME}>
    <PixiePodsTab controlPlanePods={control} dataPlanePods={data} />
  </ThemeProvider>
);

const setup = async ({ control, data }: { control?: GQLPodStatus[], data?: GQLPodStatus[] } = {}) => {
  let container: HTMLElement;
  await act(async () => {
    ({ container } = render(
      <Tester
        control={control === undefined ? mockControlPlanePods : control}
        data={data === undefined ? mockDataPlanePods : data}
      />,
    ));
  });
  await waitFor(() => {});
  return container;
};

describe('<PixiePodsTab />', () => {
  it('renders both control plane and data plane tables', async () => {
    await setup();
    await screen.findByText('control-pod-foo');
    await screen.findByText('control-pod-bar');
    await screen.findByText('data-pod-foo');
    await screen.findByText('data-pod-bar');
  });

  it('shows placeholder text when either group of pods is empty', async () => {
    const container = await setup({ control: [], data: [] });
    await screen.findByText('Cluster has no Pixie control plane pods.');
    await screen.findByText('Cluster has no unhealthy Pixie data plane pods.');
    expect(container.querySelectorAll('tbody tr').length).toBe(0);
  });

  it('does not explode when props are missing', async () => {
    const container = await setup({ control: null, data: null });
    await screen.findByText('Cluster has no Pixie control plane pods.');
    await screen.findByText('Cluster has no unhealthy Pixie data plane pods.');
    expect(container.querySelectorAll('tbody tr').length).toBe(0);
  });

  it('counts how many healthy/unhealthy pods of each group are present', async () => {
    const container = await setup();
    const hp = container.querySelectorAll('[aria-label="Healthy pods"]');
    const up = container.querySelectorAll('[aria-label="Unhealthy pods"]');

    expect(hp.length).toBe(1);
    expect(up.length).toBe(2);
    await within(hp[0] as HTMLElement).findByText('1'); // Control plane healthy pods
    await within(up[0] as HTMLElement).findByText('1'); // Control plane unhealthy pods
    await within(up[1] as HTMLElement).findByText('2'); // Data plane unhealthy pods
  });

  it('does not show details before a pod is clicked', async () => {
    await setup();
    await screen.findByTestId('cluster-details-pods-sidebar-hidden');
    expect(screen.queryByTestId('cluster-details-pods-sidebar')).toBeNull();
  });

  it('shows events and containers within a pod when details are clicked', async () => {
    const container = await setup();
    await act(async () => {
      fireEvent.click(await within(container).findByText('control-pod-foo'));
    });
    const detailsContainer = await screen.findByTestId('cluster-details-pods-sidebar');
    expect(screen.queryByTestId('cluster-details-pods-sidebar-hidden')).toBeNull();
    await within(detailsContainer).findByText('control-pod-foo-foo');
    await within(detailsContainer).findByText('Something probably good happened');
  });

  it('shows placeholder text when there are no events or no containers', async () => {
    const container = await setup({
      control: mockControlPlanePods.map(p => ({ ...p, containers: [], events: [] })),
      data: mockDataPlanePods.map(p => ({ ...p, containers: [], events: [] })),
    });
    await act(async () => {
      fireEvent.click(await within(container).findByText('control-pod-foo'));
    });
    const detailsContainer = await screen.findByTestId('cluster-details-pods-sidebar');
    expect(screen.queryByTestId('cluster-details-pods-sidebar-hidden')).toBeNull();
    await within(detailsContainer).findByText('No events to report.');
    await within(detailsContainer).findByText('No containers in this pod.');
  });

  it('shows placeholder text when event and container data is absent', async () => {
    const container = await setup({
      control: mockControlPlanePods.map(p => ({ ...p, containers: null, events: null })),
      data: mockDataPlanePods.map(p => ({ ...p, containers: null, events: null })),
    });
    await act(async () => {
      fireEvent.click(await within(container).findByText('control-pod-foo'));
    });
    const detailsContainer = await screen.findByTestId('cluster-details-pods-sidebar');
    expect(screen.queryByTestId('cluster-details-pods-sidebar-hidden')).toBeNull();
    await within(detailsContainer).findByText('No events to report.');
    await within(detailsContainer).findByText('No containers in this pod.');
  });

  it('switches which pod details to show when another is clicked', async () => {
    const container = await setup();
    await act(async () => {
      fireEvent.click(await within(container).findByText('control-pod-foo'));
    });
    await act(async () => {
      fireEvent.click(await within(container).findByText('data-pod-bar'));
    });
    const detailsContainer = await screen.findByTestId('cluster-details-pods-sidebar');
    expect(screen.queryByTestId('cluster-details-pods-sidebar-hidden')).toBeNull();
    await within(detailsContainer).findByText('data-pod-bar-foo');
    await within(detailsContainer).findByText('Bad news');
  });

  it('stops showing details when the currently-shown row is clicked again', async () => {
    const container = await setup();

    // Once for control plane
    await act(async () => {
      fireEvent.click(await within(container).findByText('control-pod-foo'));
    });
    await screen.findByTestId('cluster-details-pods-sidebar');
    await act(async () => {
      // We want the td, not the h1 in the details sidebar
      fireEvent.click((await within(container).findAllByText('control-pod-foo'))[0]);
    });
    await screen.findByTestId('cluster-details-pods-sidebar-hidden');
    expect(screen.queryByTestId('cluster-details-pods-sidebar')).toBeNull();

    // And once for data plane (different branches for coverage)
    await act(async () => {
      fireEvent.click(await within(container).findByText('data-pod-foo'));
    });
    await screen.findByTestId('cluster-details-pods-sidebar');
    await act(async () => {
      // We want the td, not the h1 in the details sidebar
      fireEvent.click((await within(container).findAllByText('data-pod-foo'))[0]);
    });
    await screen.findByTestId('cluster-details-pods-sidebar-hidden');
    expect(screen.queryByTestId('cluster-details-pods-sidebar')).toBeNull();
  });
});
