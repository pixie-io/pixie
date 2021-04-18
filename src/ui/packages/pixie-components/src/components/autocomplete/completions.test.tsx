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

import { Completion, Completions } from './completions';

const noop = () => {};

describe('<Completions/> test', () => {
  it('renders', () => {
    const wrapper = shallow(
      <Completions
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
      />,
    );
    expect(wrapper.find(Completion)).toHaveLength(5);
    expect(wrapper.findWhere((node) => node.prop('active')).prop('id')).toBe(
      'px-1',
    );
  });
});

describe('<Completion> test', () => {
  describe('renders highlights', () => {
    it('from the beginning', () => {
      const wrapper = shallow(
        <Completion
          id='some id'
          title='0123456789'
          highlights={[0, 1, 2, 4]}
          active={false}
          onActiveChange={noop}
          onSelection={noop}
        />,
      );
      const child = wrapper.find('CompletionInternal').shallow();
      expect(child.find('span').at(0).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(1).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(2).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(3).hasClass(/highlight/)).toEqual(false);
      expect(child.find('span').at(4).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(5).hasClass(/highlight/)).toEqual(false);
    });

    it('in the middle', () => {
      const wrapper = shallow(
        <Completion
          id='some id'
          title='0123456789'
          highlights={[1, 2]}
          active={false}
          onActiveChange={noop}
          onSelection={noop}
        />,
      );
      const child = wrapper.find('CompletionInternal').shallow();
      expect(child.find('span').at(0).hasClass(/highlight/)).toEqual(false);
      expect(child.find('span').at(1).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(2).hasClass(/highlight/)).toEqual(true);
      expect(child.find('span').at(3).hasClass(/highlight/)).toEqual(false);
    });
  });
});
