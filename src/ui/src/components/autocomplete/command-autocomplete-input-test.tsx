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
import { render } from '@testing-library/react';

import { CommandAutocompleteInput } from 'app/components/autocomplete/command-autocomplete-input';

const noop = () => {};

describe('<AutcompleteInput/> test', () => {
  it('renders the correct spans', () => {
    const { container } = render(
      <CommandAutocompleteInput
        onKey={noop}
        onChange={noop}
        setCursor={noop}
        cursorPos={8}
        placeholder='test'
        isValid={false}
        value={[
          {
            type: 'key',
            value: 'svc: ',
          },
          {
            type: 'value',
            value: 'pl/test',
          },
        ]}
      />,
    );
    const spans = container.querySelectorAll('span');
    expect(spans).toHaveLength(4);
    expect(spans[0].innerHTML).toBe('svc: ');
    expect(spans[1].innerHTML).toBe('pl/');
    expect(spans[2].innerHTML).toBe('test');
    expect(spans[3].innerHTML).toBe(''); // Placeholder span.
  });

  it('renders placeholder', () => {
    const { container } = render(
      <CommandAutocompleteInput
        onKey={noop}
        onChange={noop}
        setCursor={noop}
        cursorPos={0}
        placeholder='test'
        value={[]}
        isValid={false}
      />,
    );
    expect(container.querySelectorAll('span')).toHaveLength(1);
    expect(container.querySelector('span').innerHTML).toBe('test'); // Placeholder span.
  });
});
