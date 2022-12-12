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

import { CommandProvider } from 'app/components/command-palette/providers/command-provider';
import { GQLAutocompleteEntityKind } from 'app/types/schema';

/**
 * On an empty input, suggests commonly-used script commands. On a non-empty input, doesn't suggest anything.
 */
export const useEmptyInputScriptProvider: CommandProvider = () => {
  return React.useCallback(async (input: string) => {
    return {
      providerName: 'Suggested Scripts',
      hasAdditionalMatches: false,
      completions: !input.trim().length ? [{
        key: GQLAutocompleteEntityKind.AEK_SCRIPT,
        label: 'script:px/cluster',
        description: 'Read the high-level status of your cluster: namespaces, pods, performance metrics, etc.',
        onSelect: () => ['script:px/cluster start_time:-5m', 32],
      }] : [], // No suggestions if the input has meaningful text in it already.
    };
  }, []);
};
