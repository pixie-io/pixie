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
// eslint-disable-next-line import/no-extraneous-dependencies
import { screen, render } from '@testing-library/react';
import { ThemeProvider } from '@material-ui/core/styles';
import { DARK_THEME } from 'app/components';
import { Breadcrumbs } from './breadcrumbs';

describe('<Breadcrumbs/>', () => {
  it('renders correctly', async () => {
    const breadcrumbs = [
      {
        title: 'cluster',
        value: 'gke-prod',
        selectable: true,
        allowTyping: false,
        getListItems: async (input) => {
          if (input) {
            return [
              { value: 'cluster1' },
              { value: 'cluster2' },
              { value: 'cluster3' },
            ];
          }
          return [
            { value: 'cluster1' },
            { value: 'cluster2' },
            { value: 'cluster3' },
          ];
        },
      },
      {
        title: 'pod',
        value: 'pod-123',
        selectable: true,
        allowTyping: true,
        getListItems: async (input) => {
          if (input === '') {
            return [{ value: 'pod1' }, { value: 'pod2' }];
          }
          return [
            { value: 'some pod' },
            { value: 'another pod' },
            { value: 'pod' },
          ];
        },
      },
      {
        title: 'script',
        value: 'px/pod',
        selectable: false,
      },
    ];

    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <Breadcrumbs breadcrumbs={breadcrumbs} />
      </ThemeProvider>,
    );
    await screen.findByText('cluster:'); // Entirely to wait for actions
    expect(container).toMatchSnapshot();
  });
});
