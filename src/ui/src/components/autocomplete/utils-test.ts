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

import * as utils from './utils';

describe('GetDisplayStringFromTabStops', () => {
  it('should return the correct string', () => {
    const ts = [
      {
        Index: 2,
        Label: 'svc_name',
        Value: 'pl/test',
        CursorPosition: 0,
      },
      {
        Index: 3,
        Label: 'pod',
        Value: 'pl/pod',
        CursorPosition: -1,
      },
      {
        Index: 4,
        Value: 'test',
        CursorPosition: -1,
      },
    ];
    expect(utils.getDisplayStringFromTabStops(ts)).toEqual(
      'svc_name:pl/test pod:pl/pod test',
    );
  });
});

describe('TabStopParser', () => {
  const ts = [
    {
      Index: 2,
      Label: 'svc_name',
      Value: 'pl/test',
      CursorPosition: 4,
    },
    {
      Index: 3,
      Label: 'pod',
      Value: 'pl/pod',
      CursorPosition: -1,
    },
    {
      Index: 4,
      Value: 'test',
      CursorPosition: -1,
    },
  ];
  const parsedTS = new utils.TabStopParser(ts);

  it('should parse correct info', () => {
    expect(parsedTS.getInitialCursor()).toEqual(13);
    expect(parsedTS.getTabBoundaries()).toEqual([
      [9, 17],
      [21, 28],
      [28, 33],
    ]);
    expect(parsedTS.getInput()).toEqual([
      { type: 'key', value: 'svc_name:' },
      { type: 'value', value: 'pl/test' },
      { type: 'value', value: ' ' },
      { type: 'key', value: 'pod:' },
      { type: 'value', value: 'pl/pod' },
      { type: 'value', value: ' ' },
      { type: 'value', value: 'test' },
    ]);
  });

  it('should getActiveTab', () => {
    expect(parsedTS.getActiveTab(9)).toEqual(0);
    expect(parsedTS.getActiveTab(10)).toEqual(0);
    expect(parsedTS.getActiveTab(17)).toEqual(1);
    expect(parsedTS.getActiveTab(24)).toEqual(1);
  });

  it('should handleCompletionSelection', () => {
    expect(parsedTS.handleCompletionSelection(9, {
      type: 'script',
      title: 'testScript',
    })).toEqual(['svc_name:testScript pod:pl/pod test', 18]);
    expect(parsedTS.handleCompletionSelection(23, {
      type: 'script',
      title: 'testScript',
    })).toEqual(['svc_name:pl/test pod:testScript test', 31]);
    expect(parsedTS.handleCompletionSelection(29, {
      type: 'script',
      title: 'testScript',
    })).toEqual(['svc_name:pl/test pod:pl/pod script:testScript', 45]);
  });

  it('should backspace', () => {
    expect(parsedTS.handleBackspace(9)).toEqual([
      ' pod:pl/pod test',
      0,
      [
        {
          Index: 2,
          CursorPosition: 0,
        },
        {
          Index: 3,
          Label: 'pod',
          Value: 'pl/pod',
          CursorPosition: -1,
        },
        {
          Index: 4,
          Value: 'test',
          CursorPosition: -1,
        },
      ],
      false,
    ]);
    expect(parsedTS.handleBackspace(21)).toEqual([
      'svc_name:pl/test test',
      16,
      [
        {
          Index: 2,
          Label: 'svc_name',
          Value: 'pl/test',
          CursorPosition: 7,
        },
        {
          Index: 4,
          Value: 'test',
          CursorPosition: -1,
        },
      ],
      true,
    ]);
    expect(parsedTS.handleBackspace(10)).toEqual([
      'svc_name:l/test pod:pl/pod test',
      9,
      [
        {
          Index: 2,
          Label: 'svc_name',
          Value: 'l/test',
          CursorPosition: 0,
        },
        {
          Index: 3,
          Label: 'pod',
          Value: 'pl/pod',
          CursorPosition: -1,
        },
        {
          Index: 4,
          Value: 'test',
          CursorPosition: -1,
        },
      ],
      false,
    ]);
    expect(parsedTS.handleBackspace(28)).toEqual([
      'svc_name:pl/test pod:pl/pod',
      27,
      [
        {
          Index: 2,
          Label: 'svc_name',
          Value: 'pl/test',
          CursorPosition: -1,
        },
        {
          Index: 3,
          Label: 'pod',
          Value: 'pl/pod',
          CursorPosition: 6,
        },
      ],
      true,
    ]);
  });

  it('should handleChange', () => {
    expect(
      parsedTS.handleChange('svc_name:apl/test pod:pl/pod test', 10),
    ).toEqual([
      {
        Index: 2,
        Label: 'svc_name',
        Value: 'apl/test',
        CursorPosition: 1,
      },
      {
        Index: 3,
        Label: 'pod',
        Value: 'pl/pod',
        CursorPosition: -1,
      },
      {
        Index: 4,
        Value: 'test',
        CursorPosition: -1,
      },
    ]);
  });
});
