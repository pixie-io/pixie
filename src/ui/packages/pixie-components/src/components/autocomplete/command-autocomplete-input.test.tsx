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

import { shallow } from 'enzyme';
import * as React from 'react';

import { CommandAutocompleteInput } from 'components/autocomplete/command-autocomplete-input';

const noop = () => {};

describe('<AutcompleteInput/> test', () => {
  it('renders the correct spans', () => {
    const wrapper = shallow(
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
    expect(wrapper.find('span')).toHaveLength(4);
    expect(wrapper.find('span').at(0).text()).toBe('svc: ');
    expect(wrapper.find('span').at(1).text()).toBe('pl/');
    expect(wrapper.find('span').at(2).text()).toBe('test');
    expect(wrapper.find('span').at(3).text()).toBe(''); // Placeholder span.
  });

  it('renders placeholder', () => {
    const wrapper = shallow(
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
    expect(wrapper.find('span')).toHaveLength(1);
    expect(wrapper.find('span').at(0).text()).toBe('test'); // Placeholder span.
  });
});
