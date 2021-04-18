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

import { pluralize } from './pluralize';

describe('pluralize test', () => {
  it('should return singular if the count is 1', () => {
    expect(pluralize('word', 1)).toEqual('word');
  });

  it('should return plural if the count is 0', () => {
    expect(pluralize('word', 0)).toEqual('words');
  });

  it('should return plural if the count is greater than 1', () => {
    expect(pluralize('word', 10)).toEqual('words');
  });

  it('should return the given plural if the count is greater than 1', () => {
    expect(pluralize('person', 10, 'people')).toEqual('people');
  });
});
