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

import { JSONData } from './json-data';

/* eslint-disable react-memo/require-usememo */

describe('<JSONData/> test', () => {
  it('should render correctly for single line', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <JSONData
          data={{
            testString: 'a',
            testNum: 10,
            testNull: null,
            testJSON: {
              hello: 'world',
            },
          }}
        />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render correctly for multiline', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <JSONData
          data={{
            testString: 'a',
            testNum: 10,
            testNull: null,
            testJSON: {
              hello: 'world',
            },
          }}
          multiline
        />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render array correctly for single line', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <JSONData
          data={['some text', 'some other text']}
        />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });

  it('should render array correctly for multiline', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <JSONData
          data={[
            { a: 1, b: { c: 'foo' } },
            { a: 3, b: null },
          ]}
          multiline
        />
      </ThemeProvider>,
    );

    expect(container).toMatchSnapshot();
  });
});
