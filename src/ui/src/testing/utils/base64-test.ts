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

import { atou } from './base64';

describe('Script execution helpers', () => {
  it('Can parse base64 correctly', () => {
    const results: Array<[string, number[]]> = [
      ['AAAAR/AK', [0, 0, 0, 0x47, 0xF0, 0x0A]],
      ['AAAAR/AKB===', [0, 0, 0, 0x47, 0xF0, 0x0A, 0x04, 0, 0]],
      ['A===', [0, 0, 0]],
    ];
    for (const [input, output] of results) {
      const arr = atou(input);
      expect([].slice.call(arr)).toEqual(output);
    }
  });

  it('Throws on zero-length strings', () => {
    expect(() => atou('')).toThrow(
      'Incorrect base64 encoding: must be a string with non-zero length');
  });

  it('Throws on strings with invalid base64 characters', () => {
    expect(() => atou('foo_bar')).toThrow(
      'Incorrect base64 encoding: may only contain A-Z, a-z, 0-9, +, /, and = characters.');
  });

  it('Throws on inputs with length not divisible by 4', () => {
    expect(() => atou('abcde')).toThrow(
      'Incorrect base64 encoding: length not divisible by 4.');
  });

  it('Throws on inputs with padding in the wrong places', () => {
    expect(() => atou('a=bcd===')).toThrow(
      'Incorrect base64 encoding: padding characters may only appear at the end.');
  });

  it('Throws on inputs with excessive padding', () => {
    expect(() => atou('abcd====')).toThrow(
      'Incorrect base64 encoding: too many padding characters (max 3).');
  });
});
