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

import { normalize, highlightMatch } from './string-search';

describe('String search utils', () => {
  it('Normalizes strings', () => {
    expect(normalize('')).toBe('');
    expect(normalize('Foo')).toBe('foo');
    expect(normalize('   pX/b  __e-*sPo.\tkE123\r\n')).toBe('px/bespoke123');
  });

  it('Does not highlight if there was no match', () => {
    expect(highlightMatch('', '')).toEqual([]);
    expect(highlightMatch('', 'foo')).toEqual([]);
    expect(highlightMatch('search', '')).toEqual([]);
    expect(highlightMatch('search', 'foo')).toEqual([]);
  });

  it('Does not highlight if there was a partial match', () => {
    expect(highlightMatch('foo', 'for')).toEqual([]);
    expect(highlightMatch('hello', 'heck')).toEqual([]);
  });

  it('Highlights the first configuration it finds', () => {
    expect(highlightMatch('foo', 'foo')).toEqual([0, 1, 2]);
    expect(highlightMatch('wor', 'Hello, World!')).toEqual([7, 8, 9]);
    expect(highlightMatch('Q_LsT', 'px/sql_stats')).toEqual([4, 5, 7, 8]);
  });
});
