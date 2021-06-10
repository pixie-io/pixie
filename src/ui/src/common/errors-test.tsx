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

// eslint-disable-next-line import/no-extraneous-dependencies
import { shallow } from 'enzyme';
import * as React from 'react';

import { VizierQueryError } from 'app/api';
import { VizierErrorDetails } from './errors';

describe('<VizierErrorDetails/> test', () => {
  it('renders the details if it is a VizierQueryError', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new VizierQueryError('server', 'a well formatted server error')
      }
      />,
    );
    expect(wrapper.find('div').at(0).text()).toBe('a well formatted server error');
  });

  it('renders a list of errors if the details is a list', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new VizierQueryError('script', ['error 1', 'error 2', 'error 3'])
      }
      />,
    );
    expect(wrapper.find('div').length).toBe(3);
  });

  it('renders the message for other errors', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new Error('generic error')
      }
      />,
    );
    expect(wrapper.find('div').at(0).text()).toBe('generic error');
  });
});
