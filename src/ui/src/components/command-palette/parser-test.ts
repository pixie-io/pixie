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

import { parse, ParseResult } from './parser';

function testParse(input: string, selection: [start: number, end: number], expected: ParseResult) {
  const out = parse(input, selection);
  expect(out).toEqual(expected);
}

/* eslint-disable max-len */
const validInputResults: ParseResult[] = [
  {
    input: '',
    selection: [0, 0],
    tokens: [],
    selectedTokens: [],
    kvMap: null,
  },
  {
    input: 'key',
    selection: [0, 0],
    tokens: [
      { type: 'value', index: 0, text: 'key', value: 'key', start: 0, end: 3 },
    ],
    selectedTokens: [
      { token: { type: 'value', index: 0, text: 'key', value: 'key', start: 0, end: 3 }, selectionStart: 0, selectionEnd: 0 },
    ],
    kvMap: null,
  },
  {
    input: 'key:',
    selection: [0, 0],
    tokens: [
      { type: 'key', index: 0, text: 'key', value: 'key', start: 0, end: 3 },
      { type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4, relatedToken: { type: 'key', index: 0, text: 'key', value: 'key', start: 0, end: 3 } },
    ],
    selectedTokens: [
      { token: { type: 'key', index: 0, text: 'key', value: 'key', start: 0, end: 3 }, selectionStart: 0, selectionEnd: 0 },
    ],
    kvMap: new Map([['key', '']]),
  },
  {
    input: 'foo:bar',
    selection: [4, 4],
    tokens: [
      { type: 'key', index: 0, text: 'foo', value: 'foo', start: 0, end: 3, relatedToken: expect.objectContaining({ value: 'bar' }) },
      { type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4, relatedToken: expect.objectContaining({ value: 'foo' }) },
      { type: 'value', index: 2, text: 'bar', value: 'bar', start: 4, end: 7, relatedToken: expect.objectContaining({ value: 'foo' }) },
    ],
    selectedTokens: [
      { token: expect.objectContaining({ type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4 }), selectionStart: 1, selectionEnd: 1 },
      { token: expect.objectContaining({ type: 'value', index: 2, text: 'bar', value: 'bar', start: 4, end: 7 }), selectionStart: 0, selectionEnd: 0 },
    ],
    kvMap: new Map([['foo', 'bar']]),
  },
  {
    input: 'foo:"quoted escaped\\"string"  bar:baz',
    selection: [0, 37],
    tokens: [
      { type: 'key', index: 0, text: 'foo', value: 'foo', start: 0, end: 3, relatedToken: expect.objectContaining({ value: 'quoted escaped"string' }) },
      { type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4, relatedToken: expect.objectContaining({ value: 'foo' }) },
      { type: 'value', index: 2, text: '"quoted escaped\\"string"', value: 'quoted escaped"string', start: 4, end: 28, relatedToken: expect.objectContaining({ value: 'foo' }) },
      { type: 'none', index: 3, text: '  ', value: '', start: 28, end: 30 },
      { type: 'key', index: 4, text: 'bar', value: 'bar', start: 30, end: 33, relatedToken: expect.objectContaining({ value: 'baz' }) },
      { type: 'eq', index: 5, text: ':', value: '', start: 33, end: 34, relatedToken: expect.objectContaining({ value: 'bar' }) },
      { type: 'value', index: 6, text: 'baz', value: 'baz', start: 34, end: 37, relatedToken: expect.objectContaining({ value: 'bar' }) },
    ],
    selectedTokens: [
      { token: { type: 'key', index: 0, text: 'foo', value: 'foo', start: 0, end: 3, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 3 },
      { token: { type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 1 },
      {
        token: { type: 'value', index: 2, text: '"quoted escaped\\"string"', value: 'quoted escaped"string', start: 4, end: 28, relatedToken: expect.anything() },
        selectionStart: 0,
        selectionEnd: 24,
      },
      { token: { type: 'none', index: 3, text: '  ', value: '', start: 28, end: 30 }, selectionStart: 0, selectionEnd: 2 },
      { token: { type: 'key', index: 4, text: 'bar', value: 'bar', start: 30, end: 33, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 3 },
      { token: { type: 'eq', index: 5, text: ':', value: '', start: 33, end: 34, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 1 },
      { token: { type: 'value', index: 6, text: 'baz', value: 'baz', start: 34, end: 37, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 3 },
    ],
    kvMap: new Map([['foo', 'quoted escaped"string'], ['bar', 'baz']]),
  },
];

const invalidInputResults: ParseResult[] = [
  {
    input: '\\',
    selection: [1, 1],
    tokens: [
      { type: 'error', index: 0, text: '\\', value: '', start: 0, end: 1 },
    ],
    selectedTokens: [
      { token: { type: 'error', index: 0, text: '\\', value: '', start: 0, end: 1 }, selectionStart: 1, selectionEnd: 1 },
    ],
    kvMap: null,
  },
  {
    input: '\\"',
    selection: [1, 1],
    tokens: [
      { type: 'error', index: 0, text: '\\"', value: '', start: 0, end: 2 },
    ],
    selectedTokens: [
      { token: { type: 'error', index: 0, text: '\\"', value: '', start: 0, end: 2 }, selectionStart: 1, selectionEnd: 1 },
    ],
    kvMap: null,
  },
  {
    input: 'quotes:\\"missing\\"',
    selection: [18, 18],
    tokens: [
      { type: 'key', index: 0, text: 'quotes', value: 'quotes', start: 0, end: 6 },
      { type: 'eq', index: 1, text: ':', value: '', start: 6, end: 7, relatedToken: expect.objectContaining({ value: 'quotes' }) },
      { type: 'error', index: 2, text: '\\"', value: '', start: 7, end: 9 },
      { type: 'value', index: 3, text: 'missing', value: 'missing', start: 9, end: 16 },
      { type: 'error', index: 4, text: '\\"', value: '', start: 16, end: 18 },
    ],
    selectedTokens: [
      { token: { type: 'error', index: 4, text: '\\"', value: '', start: 16, end: 18 }, selectionStart: 2, selectionEnd: 2 },
    ],
    kvMap: new Map([['quotes', '']]),
  },
  {
    input: 'foo:bar foo:duplicate',
    selection: [0, 0],
    tokens: [
      { type: 'key', index: 0, text: 'foo', value: 'foo', start: 0, end: 3, relatedToken: expect.objectContaining({ value: 'bar' }) },
      { type: 'eq', index: 1, text: ':', value: '', start: 3, end: 4, relatedToken: expect.objectContaining({ index: 0 }) },
      { type: 'value', index: 2, text: 'bar', value: 'bar', start: 4, end: 7, relatedToken: expect.objectContaining({ index: 0 }) },
      { type: 'none', index: 3, text: ' ', value: '', start: 7, end: 8 },
      { type: 'error', index: 4, text: 'foo', value: 'foo', start: 8, end: 11 },
      { type: 'error', index: 5, text: ':', value: '', start: 11, end: 12 },
      { type: 'error', index: 6, text: 'duplicate', value: 'duplicate', start: 12, end: 21 },
    ],
    selectedTokens: [
      { token: { type: 'key', index: 0, text: 'foo', value: 'foo', start: 0, end: 3, relatedToken: expect.anything() }, selectionStart: 0, selectionEnd: 0 },
    ],
    kvMap: new Map([['foo', 'bar']]),
  },
];
/* eslint-disable max-len */

describe('Command Input parser', () => {
  it('Parses a variety of valid inputs and follows the caret', () => {
    for (const expected of validInputResults) {
      testParse(expected.input, expected.selection, expected);
    }
  });

  it('Parses a variety of invalid inputs and follows the caret', () => {
    for (const expected of invalidInputResults) {
      testParse(expected.input, expected.selection, expected);
    }
  });
});
