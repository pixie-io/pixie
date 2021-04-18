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
import {
  AUTOCOMPLETE_QUERIES,
  GQLAutocompleteActionType, GQLAutocompleteResult,
} from '@pixie-labs/api';
import { useApolloClient } from '@apollo/client';

export type AutocompleteSuggester = (
  input: string, cursor: number, action: GQLAutocompleteActionType
) => Promise<GQLAutocompleteResult>;

export function useAutocomplete(clusterUID: string): AutocompleteSuggester {
  const client = useApolloClient();
  return React.useCallback((input: string, cursor: number, action: GQLAutocompleteActionType) => (
    client.query<{ autocomplete: GQLAutocompleteResult }>({
      query: AUTOCOMPLETE_QUERIES.AUTOCOMPLETE,
      fetchPolicy: 'network-only',
      variables: {
        input,
        cursor,
        action,
        clusterUID,
      },
    }).then(
      (results) => results.data.autocomplete,
    )), [client, clusterUID]);
}
