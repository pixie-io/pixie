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

import { Completion, Completions } from './completions';

const noop = () => {};

describe('<Completions/> test', () => {
  it('renders', () => {
    const { container } = render(
      <ThemeProvider theme={DARK_THEME}>
        <Completions
          // eslint-disable-next-line react-memo/require-usememo
          items={[
            { type: 'header', header: 'Recently used' },
            {
              type: 'item',
              title: 'px/script1',
              id: 'px-0',
              highlights: [3, 4, 5],
            },
            {
              type: 'item',
              title: 'px/script2',
              id: 'px-1',
              highlights: [3, 4, 5],
            },
            {
              type: 'item',
              title: 'px/script3',
              id: 'px-2',
              highlights: [3, 4, 5],
            },
            { type: 'header', header: 'Org scripts' },
            { type: 'item', title: 'my-org/script1', id: 'my-org-4' },
            { type: 'item', title: 'my-org/script2', id: 'my-org-5' },
          ]}
          onActiveChange={noop}
          activeItem='px-1'
          onSelection={noop}
        />
      </ThemeProvider>,
    );
    expect(container).toMatchSnapshot();
  });
});

describe('<Completion> test', () => {
  describe('renders highlights', () => {
    it('from the beginning', () => {
      const { container } = render(
        <ThemeProvider theme={DARK_THEME}>
          <Completion
            id='some id'
            title='0123456789'
            // eslint-disable-next-line react-memo/require-usememo
            highlights={[0, 1, 2, 4]}
            active={false}
            onActiveChange={noop}
            onSelection={noop}
          />
        </ThemeProvider>,
      );
      expect(container).toMatchSnapshot();
    });

    it('in the middle', () => {
      const { container } = render(
        <ThemeProvider theme={DARK_THEME}>
          <Completion
            id='some id'
            title='0123456789'
            // eslint-disable-next-line react-memo/require-usememo
            highlights={[1, 2]}
            active={false}
            onActiveChange={noop}
            onSelection={noop}
          />
        </ThemeProvider>,
      );
      expect(container).toMatchSnapshot();
    });
  });
});
