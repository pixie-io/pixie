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

import { getClasses } from '@material-ui/core/test-utils';
import { mount } from 'enzyme';
import * as React from 'react';
import {
  AlertData, DurationRenderer, JSONData, formatBytes, formatDuration,
} from './format-data';

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
  const baseComponent = DurationRenderer({ data: 20 * 1000 * 1000 });
  const classes = getClasses(baseComponent);

  it('should render correctly for low latency', () => {
    const component = DurationRenderer({ data: 20 * 1000 * 1000 });
    const wrapper = mount(component);

    expect(wrapper.find('div').prop('className')).toEqual(classes.low);
    expect(wrapper.find('div').text()).toEqual('20\u00a0ms');
  });

  it('should render correctly for medium latency', () => {
    const component = DurationRenderer({ data: 250 * 1000 * 1000 });
    const wrapper = mount(component);

    expect(wrapper.find('div').prop('className')).toEqual(classes.med);
    expect(wrapper.find('div').text()).toEqual('250\u00a0ms');
  });

  it('should render correctly for high latency', () => {
    const component = DurationRenderer({ data: 400 * 1000 * 1000 });
    const wrapper = mount(component);

    expect(wrapper.find('div').prop('className')).toEqual(classes.high);
    expect(wrapper.find('div').text()).toEqual('400\u00a0ms');
  });
});

describe('<AlertData/> test', () => {
  const classes = getClasses(<AlertData data />);

  it('should render correctly for true alert', () => {
    const wrapper = mount(<AlertData data />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.true);
    expect(wrapper.find('div').text()).toEqual('true');
  });

  it('should render correctly for false alert', () => {
    const wrapper = mount(<AlertData data={false} />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.false);
    expect(wrapper.find('div').text()).toEqual('false');
  });
});

describe('<JSONData/> test', () => {
  const classes = getClasses(
    <JSONData
      data={{
        testString: 'a',
        testNum: 10,
        testNull: null,
        testJSON: {
          hello: 'world',
        },
      }}
    />,
  );

  it('should render correctly for single line', () => {
    const wrapper = mount(
      <JSONData
        data={{
          testString: 'a',
          testNum: 10,
          testNull: null,
          testJSON: {
            hello: 'world',
          },
        }}
      />,
    );

    expect(wrapper.text()).toEqual('{\u00A0testString:\u00A0a,\u00A0testNum:\u00A010,'
      + '\u00A0testNull:\u00A0null,\u00A0testJSON:\u00A0{\u00A0hello:\u00A0world\u00A0}\u00A0}');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual(classes.base);

    const topLevelJSONContents = base.children();
    expect(topLevelJSONContents).toHaveLength(6);
    expect(topLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(5);
    expect(topLevelJSONContents.find({ className: classes.number })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.null })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.string })).toHaveLength(2);
    expect(topLevelJSONContents.find({ className: classes.base })).toHaveLength(1);

    const innerLevelJSONContents = topLevelJSONContents.find({
      className: classes.base,
    }).at(0).children();
    expect(innerLevelJSONContents.find({ className: classes.string })).toHaveLength(1);
    expect(innerLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(1);
  });

  it('should render correctly for multiline', () => {
    const wrapper = mount(
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
      />,
    );

    expect(wrapper.text()).toEqual('{\u00A0testString:\u00A0a,\u00A0testNum:\u00A010,\u00A0testNull:\u00A0'
      + 'null,\u00A0testJSON:\u00A0{\u00A0hello:\u00A0world\u00A0}\u00A0}');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual(classes.base);
    const topLevelJSONContents = base.children();

    expect(topLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(5);
    expect(topLevelJSONContents.find({ className: classes.number })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.null })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.string })).toHaveLength(2);
    expect(topLevelJSONContents.find({ className: classes.base })).toHaveLength(1);
    expect(topLevelJSONContents.find('br')).toHaveLength(7);

    const innerLevelJSONContents = topLevelJSONContents.find({
      className: classes.base,
    }).at(0).children();
    expect(innerLevelJSONContents.find({ className: classes.string })).toHaveLength(1);
    expect(innerLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(1);

    expect(wrapper.find(JSONData).at(1).props().multiline).toEqual(true);
    expect(wrapper.find(JSONData).at(1).props().indentation).toEqual(1);
  });

  it('should render array correctly for single line', () => {
    const wrapper = mount(
      <JSONData
        data={['some text', 'some other text']}
      />,
    );

    expect(wrapper.text()).toEqual('[\u00A0some\u00A0text,\u00A0some\u00A0other\u00A0text\u00A0]');
    expect(wrapper.find('br')).toHaveLength(0);
  });

  it('should render array correctly for multiline', () => {
    const wrapper = mount(
      <JSONData
        data={[
          { a: 1, b: { c: 'foo' } },
          { a: 3, b: null },
        ]}
        multiline
      />,
    );

    expect(wrapper.text()).toEqual('[\u00A0{\u00A0a:\u00A01,\u00A0b:\u00A0{\u00A0c:\u00A0foo\u00A0}\u00A0},'
      + '\u00A0{\u00A0a:\u00A03,\u00A0b:\u00A0null\u00A0}\u00A0]');
    expect(wrapper.find('br')).toHaveLength(11);
  });
});
