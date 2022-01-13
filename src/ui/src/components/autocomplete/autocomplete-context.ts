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

import * as React from 'react';

export interface AutocompleteContextProps {
  /** @default true */
  allowTyping?: boolean;
  /** @default true */
  requireCompletion?: boolean;
  /**
   * What to do when an associated input is "opened" - like first being created, or being revealed in a dropdown.
   * 'none': don't respond to the input being opened
   * 'focus': moves tab focus to the input
   * 'select': moves tab focus to the input, then selects its contents as if Ctrl/Cmd+A had been pressed.
   * 'clear': moves tab focus to the input, and sets its value to the empty string.
   * @default focus
   */
  onOpen?: 'none' | 'focus' | 'select' | 'clear';
  hidden?: boolean;
  inputRef?: React.MutableRefObject<HTMLInputElement>;
}

export const AutocompleteContext = React.createContext<AutocompleteContextProps>({
  allowTyping: true,
  requireCompletion: true,
  onOpen: 'none',
  hidden: false,
});
AutocompleteContext.displayName = 'AutocompleteContext';
