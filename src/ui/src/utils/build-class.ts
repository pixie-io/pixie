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

/**
 * Utility function to create a valid CSS className from a potentially sparse list of classes.
 * Typical uses might look like these:
 * ```
 * buildClass({
 *   foo: true,
 *   bar: false,
 *   baz: true,
 * }) // returns 'foo baz'
 *
 * // returns 'classA classC' if someCondition is not satisfied. Returns 'classA classB classC' if it is.
 * buildClass('classA', someCondition && 'classB', 'classC')
 * ```
 *
 * It can also handle more exotic calls (please don't actually do this):
 * ```
 * // returns 'a b c d e'
 * buildClass('a', false, 'b', ['c', {d: 'truthy value'}], undefined, null, '', 'e')
 * ```
 * @param classes
 */
export function buildClass(...classes: any[]): string {
  return classes
    .flat(Infinity)
    .map((c) => {
      switch (typeof c) {
        case 'object':
          // typeof null === 'object', and Object.keys(null) throws an error, so we have to check for it.
          return c ? buildClass(...Object.keys(c).filter((k) => c[k])) : '';
        case 'undefined':
        case 'boolean':
          // Boolean true doesn't make sense, so treat it the same as false/undefined/empty string.
          return '';
        case 'string':
          return c;
        case 'bigint':
        case 'number':
        case 'function':
        case 'symbol':
        default:
          throw new Error(`Cannot create a classname from a ${typeof c}`);
      }
    })
    .filter((c) => c)
    .join(' ');
}
