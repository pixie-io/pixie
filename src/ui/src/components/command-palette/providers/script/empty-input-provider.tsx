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

import { PixieCommandIcon as ScriptIcon } from 'app/components';
import {
  CommandProvider,
  CommandProviderDispatchAction,
  CommandProviderState,
} from 'app/components/command-palette/providers/command-provider';
import { SCRATCH_SCRIPT } from 'app/containers/App/scripts-context';
import { GQLAutocompleteEntityKind } from 'app/types/schema';
import { checkExhaustive } from 'app/utils/check-exhaustive';

import { CompletionDescription, CompletionLabel, quoteIfNeeded } from './script-provider-common';

const DEFAULT: CommandProviderState = {
  input: '',
  selection: [0, 0],
  providerName: 'EmptyInputScriptProvider',
  loading: false,
  completions: [],
  hasAdditionalMatches: false,
};

const suggestions = [
  {
    sel: `script:${quoteIfNeeded(SCRATCH_SCRIPT.id)}`,
    label: SCRATCH_SCRIPT.title,
    desc: SCRATCH_SCRIPT.description,
  },
  {
    sel: 'script:px/cluster start_time:-5m',
    label: 'px/cluster',
    desc: 'Read the high-level status of your cluster: namespaces, pods, performance metrics, etc.',
  },
];

/**
 * On an empty input, suggests commonly-used script commands. On a non-empty input, doesn't suggest anything.
 */
export const useEmptyInputScriptProvider: CommandProvider = () => {
  return React.useReducer((prevState: CommandProviderState, action: CommandProviderDispatchAction) => {
    const { type } = action;
    switch (type) {
      case 'cancel': return prevState;
      case 'invoke': return {
        ...DEFAULT,
        input: action.input,
        selection: action.selection,
        completions: !action.input.trim().length ? suggestions.map(({ sel, label, desc }, i) => ({
          heading: 'Suggested Scripts',
          key: GQLAutocompleteEntityKind.AEK_SCRIPT + '_' + i,
          // eslint-disable-next-line react-memo/require-usememo
          label: <CompletionLabel icon={<ScriptIcon />} input={label} highlights={[]} />,
          description: <CompletionDescription title={label} body={desc} />,
          onSelect: () => [sel, sel.length] as [string, number],
        })) : [], // No suggestions if the input has meaningful text in it already.
      };
      default: checkExhaustive(type);
    }
  }, DEFAULT);
};
