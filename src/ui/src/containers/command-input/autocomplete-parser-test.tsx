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

import * as AutocompleteParser from './autocomplete-parser';

describe('AutocompleteParser test', () => {
  it('should return correct tabstops for basic command', () => {
    // eslint-disable-next-line no-template-curly-in-string
    const ts = AutocompleteParser.ParseFormatStringToTabStops('${1:run} ${2:svc_name:$0pl/test} ${3}');
    expect(ts).toEqual([
      { Index: 1, Value: 'run', CursorPosition: -1 },
      {
        Index: 2, Label: 'svc_name', Value: 'pl/test', CursorPosition: 0,
      },
      { Index: 3, CursorPosition: -1 },
    ]);
  });

  it('should return correct tabstops with empty label', () => {
    // eslint-disable-next-line no-template-curly-in-string
    const ts = AutocompleteParser.ParseFormatStringToTabStops('${1:run$0} ${2:svc_name:} ${3}');
    expect(ts).toEqual([
      { Index: 1, Value: 'run', CursorPosition: 3 },
      { Index: 2, Label: 'svc_name', CursorPosition: -1 },
      { Index: 3, CursorPosition: -1 },
    ]);
  });
});
