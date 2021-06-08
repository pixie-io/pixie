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

import { LazyPanel } from './lazy-panel';

describe('<LazyPanel/> test', () => {
  it('renders null when show is false', () => {
    const wrapper = shallow(
      <LazyPanel show={false}>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    expect(wrapper.getElement()).toBe(null);
    expect(wrapper.find('.visible')).toHaveLength(0);
  });

  it('renders content when show is true', () => {
    const wrapper = shallow(
      <LazyPanel show>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    expect(wrapper.find('.content')).toHaveLength(1);
    expect(wrapper.find('.visible')).toHaveLength(1);
  });

  it("doesn't destroy the element if show becomes false", () => {
    const wrapper = shallow(
      <LazyPanel show>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    wrapper.setProps({ show: false });
    expect(wrapper.find('.content')).toHaveLength(1);
    expect(wrapper.find('.visible')).toHaveLength(0);
  });
});
