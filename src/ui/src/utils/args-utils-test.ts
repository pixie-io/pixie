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

import { argsEquals, argsForVis, argTypesForVis } from 'app/utils/args-utils';

describe('argsEquals', () => {
  it('returns true for objects with same keys and values', () => {
    const arg1 = {
      foo: 'foo',
      bar: 'bar',
    };
    const arg2 = {
      foo: 'foo',
      bar: 'bar',
    };
    expect(argsEquals(arg1, arg2)).toBe(true);
  });

  it('returns true for the same object', () => {
    const arg1 = {
      foo: 'foo',
      bar: 'bar',
    };
    expect(argsEquals(arg1, arg1)).toBe(true);
  });

  it('returns false when one as an extra key', () => {
    const arg1 = {
      foo: 'foo',
      bar: 'bar',
    };
    const arg2 = {
      foo: 'foo',
      bar: 'bar',
      baz: 'baz',
    };
    expect(argsEquals(arg1, arg2)).toBe(false);
  });

  it('returns false when the values are different', () => {
    const arg1 = {
      foo: 'foo',
      bar: 'bar',
    };
    const arg2 = {
      foo: 'foo',
      bar: 'not bar',
    };
    expect(argsEquals(arg1, arg2)).toBe(false);
  });

  it('returns false when the keys are different', () => {
    const arg1 = {
      foo: 'foo',
      bar: 'bar',
    };
    const arg2 = {
      notFoo: 'notFoo',
      notBar: 'notBar',
    };
    expect(argsEquals(arg1, arg2)).toBe(false);
  });

  it('returns true if both inputs are null', () => {
    expect(argsEquals(null, null)).toBe(true);
  });

  it('returns false if one of the args is null', () => {
    const arg = {
      notFoo: 'notFoo',
      notBar: 'notBar',
    };
    expect(argsEquals(null, arg)).toBe(false);
    expect(argsEquals(arg, null)).toBe(false);
  });
});

describe('argsForVis', () => {
  it('filters out the args that are not part of the vis', () => {
    const vis = {
      widgets: [],
      globalFuncs: [],
      variables: [
        { name: 'foo', type: 'foo' },
      ],
    };
    const args = { foo: 'foo', bar: 'bar' };

    expect(argsForVis(vis, args)).toEqual({ foo: 'foo' });
  });

  it('handles null vis correctly', () => {
    const args = { foo: 'foo', bar: 'bar' };

    expect(argsForVis(null, args)).toEqual({});
  });

  it('fills the arg with default value if it wasn\'t specified', () => {
    const vis = {
      widgets: [],
      globalFuncs: [],
      variables: [
        { name: 'foo', type: 'foo', defaultValue: 'default foo' },
        { name: 'bar', type: 'bar', defaultValue: 'default bar' },
      ],
    };
    const args = { bar: 'bar' };

    expect(argsForVis(vis, args)).toEqual({ foo: 'default foo', bar: 'bar' });
  });
});

describe('argTypesForVis', () => {
  it('returns a map with the correct arg types', () => {
    const vis = {
      widgets: [],
      globalFuncs: [],
      variables: [
        { name: 'foo', type: 'foobar' },
      ],
    };
    expect(argTypesForVis(vis)).toEqual({ foo: 'foobar' });
  });
});
