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

import { buildClass } from 'app/utils/build-class';

describe('className builder', () => {
  it('Processes empty scenarios', () => {
    expect(buildClass()).toBe('');
    expect(buildClass(null, false, undefined, true, {}, [[{}]])).toBe('');
  });

  it('Processes basic string lists', () => {
    expect(buildClass('a', 'b', 'c')).toBe('a b c');
  });

  it('Processes sparse lists with falsy values', () => {
    expect(buildClass('a', undefined, 'b', false, 'c')).toBe('a b c');
  });

  it('Processes nested lists', () => {
    expect(buildClass('a', ['b', ['c']])).toBe('a b c');
  });

  it('Processes objects', () => {
    expect(buildClass({
      a: true,
      b: false,
      c: 'truthy',
      d: null,
    })).toBe('a c');
  });

  it('Processes unreasonable requests reasonably', () => {
    expect(buildClass(
      'a',
      false,
      'b',
      [
        'c',
        { d: 'truthy value' },
      ],
      undefined,
      null,
      '',
      'e')).toBe('a b c d e');
  });

  it('Throws when given unexpected JS types', () => {
    expect(() => buildClass(1)).toThrow();
    expect(() => buildClass(() => {})).toThrow();
  });
});
