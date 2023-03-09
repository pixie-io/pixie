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

import { CommandProvider, CommandProviderState } from './providers/command-provider';
import {
  useScriptCommandIsValidProvider,
  useEmptyInputScriptProvider,
  useMissingScriptProvider,
  useScriptCommandFromKeyedValueProvider,
  useScriptCommandFromValueProvider,
} from './providers/script';

/** Hook to passively update suggestions as the input and selection change in the command palette. */
export const useCommandProviders: (
  input: string, selection: [start: number, end: number],
) => CommandProviderState[] = (
  input, selection,
) => {
  const providers: ReturnType<CommandProvider>[] = [
    useScriptCommandIsValidProvider(),
    useEmptyInputScriptProvider(),
    useMissingScriptProvider(),
    useScriptCommandFromKeyedValueProvider(),
    useScriptCommandFromValueProvider(),
  ];

  React.useEffect(() => {
    for (const p of providers) {
      p[1]({ type: 'cancel' });
      p[1]({ type: 'invoke', input, selection });
    }
    // Not monitoring changes to the providers themselves, as this would cause an infinite render loop.
    // The output of a useReducer changes whenever the state within it does, and triggering a cancel does exactly that.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [input, ...selection]);

  const states = providers.map(p => p[0]);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  return React.useMemo(() => states, [...states]);
};
