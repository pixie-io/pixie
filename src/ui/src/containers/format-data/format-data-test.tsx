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
import { render } from '@testing-library/react';

import { DARK_THEME } from 'app/components';

import {
  AlertData, DurationRenderer, formatBytes, formatDuration,
} from './format-data';

/* eslint-disable react-memo/require-usememo */
describe('formatters Test', () => {
  it('should handle 0 Bytes correctly', () => {
    expect(formatBytes(0)).toEqual({ units: '\u00a0B', val: '0' });
  });

  it('should handle 0 duration correctly', () => {
    expect(formatDuration(0)).toEqual({ units: 'ns', val: '0' });
  });
  it('should handle |x| < 1  Bytes correctly', () => {
    expect(formatBytes(0.1)).toEqual({ units: '\u00a0B', val: '0.1' });
  });

  it('should handle |x| < 1 duration correctly', () => {
    expect(formatDuration(0.1)).toEqual({ units: 'ns', val: '0.1' });
  });
  it('should handle x < 0 duration correctly', () => {
    expect(formatDuration(-2)).toEqual({ units: 'ns', val: '-2' });
  });
  it('should handle x < 0 bytes correctly', () => {
    expect(formatBytes(-2048)).toEqual({ units: 'KB', val: '-2' });
  });
  it('should handle large bytes correctly', () => {
    expect(formatBytes(1024 ** 9)).toEqual({ units: 'YB', val: '1024' });
  });
  it('should handle large durations correctly', () => {
    expect(formatDuration(1000 ** 4)).toEqual({ units: '\u00A0s', val: '1000' });
  });
});

describe('DurationRenderer test', () => {
  it('should render correctly for low latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <DurationRenderer data={20 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for medium latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <DurationRenderer data={250 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for high latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <DurationRenderer data={400 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });
});

describe('<AlertData/> test', () => {
  it('should render correctly for true alert', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <AlertData data={true} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for false alert', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <AlertData data={false} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });
});
