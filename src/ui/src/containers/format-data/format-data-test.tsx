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
  AlertData,
  BasicDurationRenderer,
  dataWithUnitsToString,
  LatencyDurationRenderer,
  formatBytes,
  formatDuration,
} from './format-data';

/* eslint-disable react-memo/require-usememo */
describe('formatDuration test', () => {
  it('should handle 0 duration correctly', () => {
    expect(formatDuration(0)).toEqual({ units: ['ns'], val: ['0'] });
  });
  it('should handle |x| < 1 duration correctly', () => {
    expect(formatDuration(0.1)).toEqual({ units: ['ns'], val: ['0.1'] });
  });
  it('should handle x < 0 duration correctly', () => {
    expect(formatDuration(-2)).toEqual({ units: ['ns'], val: ['-2'] });
  });
  it('should handle large negative durations correctly', () => {
    expect(formatDuration(-12 * 60 * 60 * 1000 * 1000 * 1000 - 1335144)).toEqual(
      { units: ['hours', 'min'], val: ['-12', '0'] });
  });
  it('should handle nanoseconds correctly', () => {
    expect(formatDuration(144)).toEqual({ units: ['ns'], val: ['144'] });
  });
  it('should handle microseconds correctly', () => {
    expect(formatDuration(5144)).toEqual({ units: ['\u00b5s'], val: ['5.1'] });
  });
  it('should handle milliseconds correctly', () => {
    expect(formatDuration(5 * 1000 * 1000)).toEqual({ units: ['ms'], val: ['5'] });
  });
  it('should handle seconds correctly', () => {
    expect(formatDuration(13 * 1000 * 1000 * 1000 + 1242)).toEqual({ units: ['\u00a0s'], val: ['13'] });
  });
  it('should handle minutes correctly', () => {
    expect(formatDuration(5 * 60 * 1000 * 1000 * 1000 + 1334)).toEqual(
      { units: ['min', 's'], val: ['5', '0'] });
  });
  it('should handle hours correctly', () => {
    expect(formatDuration(12 * 60 * 60 * 1000 * 1000 * 1000 + 1335144)).toEqual(
      { units: ['hours', 'min'], val: ['12', '0'] });
  });
  it('should handle days correctly', () => {
    expect(formatDuration(25 * 24 * 60 * 60 * 1000 * 1000 * 1000 + 133514124)).toEqual(
      { units: ['days', 'hours'], val: ['25', '0'] });
  });
});

describe('formatBytes Test', () => {
  it('should handle 0 Bytes correctly', () => {
    expect(formatBytes(0)).toEqual({ units: ['\u00a0B'], val: ['0'] });
  });
  it('should handle |x| < 1  Bytes correctly', () => {
    expect(formatBytes(0.1)).toEqual({ units: ['\u00a0B'], val: ['0.1'] });
  });
  it('should handle x < 0 bytes correctly', () => {
    expect(formatBytes(-2048)).toEqual({ units: ['KB'], val: ['-2'] });
  });
  it('should handle large bytes correctly', () => {
    expect(formatBytes(1024 ** 9)).toEqual({ units: ['YB'], val: ['1024'] });
  });
});

describe('dataWithUnitsToString test', () => {
  it('should handle 0 duration correctly', () => {
    expect(dataWithUnitsToString({ units: ['ns'], val: ['0'] })).toEqual('0 ns');
  });
  it('should handle |x| < 1 duration correctly', () => {
    expect(dataWithUnitsToString({ units: ['ns'], val: ['0.1'] })).toEqual(
      '0.1 ns');
  });
  it('should handle large durations correctly', () => {
    expect(dataWithUnitsToString({ units: ['min', 's'], val: ['10', '35'] })).toEqual(
      '10 min 35 s');
  });
  it('should handle large negative durations correctly', () => {
    expect(dataWithUnitsToString({ units: ['min', 's'], val: ['-10', '35'] })).toEqual(
      '-10 min 35 s');
  });
  it('should handle x < 0 bytes correctly', () => {
    expect(dataWithUnitsToString({ units: ['KB'], val: ['-2'] })).toEqual('-2 KB');
  });
  it('should handle bytes correctly', () => {
    expect(dataWithUnitsToString({ units: ['YB'], val: ['1024'] })).toEqual('1024 YB');
  });
});

describe('LatencyDurationRenderer test', () => {
  it('should render correctly for low latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <LatencyDurationRenderer data={20 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for medium latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <LatencyDurationRenderer data={250 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for high latency', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <LatencyDurationRenderer data={400 * 1000 * 1000} />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });
});

describe('BasicDurationRenderer test', () => {
  it('should render correctly for non latency duration', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <BasicDurationRenderer data={400 * 1000 * 1000} />
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
