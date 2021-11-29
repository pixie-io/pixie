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
import { screen, render } from '@testing-library/react';

import { VizierQueryError } from 'app/api';
import { DARK_THEME } from 'app/components';

import { VizierErrorDetails } from './errors';

/* eslint-disable react-memo/require-memo, react-memo/require-usememo */

const Tester = ({ error }) => (
  <ThemeProvider theme={DARK_THEME}>
    <VizierErrorDetails error={error} />
  </ThemeProvider>
);

describe('<VizierErrorDetails/> test', () => {
  it('renders the details if it is a VizierQueryError', async () => {
    render(
      <Tester error={
        new VizierQueryError('server', 'a well formatted server error')
      }
      />,
    );
    await screen.findByText('a well formatted server error');
  });

  it('renders a list of errors if the details is a list', async () => {
    const { container } = render(
      <Tester error={
        new VizierQueryError('script', ['error 1', 'error 2', 'error 3'])
      }
      />,
    );
    expect(container.querySelectorAll('div')).toHaveLength(3);
  });

  it('renders the message for other errors', async () => {
    render(
      <Tester error={
        new Error('generic error')
      }
      />,
    );
    await screen.findByText('generic error');
  });
});
