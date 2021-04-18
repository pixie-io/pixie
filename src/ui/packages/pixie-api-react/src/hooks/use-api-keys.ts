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
import { useQuery, useMutation } from '@apollo/client/react';
import { API_KEY_QUERIES, GQLAPIKey } from '@pixie-labs/api';
// noinspection ES6PreferShortImport
import { ImmutablePixieQueryGuaranteedResult } from '../utils/types';

interface APIKeyHookResult {
  createAPIKey: () => Promise<string>;
  deleteAPIKey: (id: string) => Promise<void>;
  /** While data is still loading, this will be undefined (to differentiate it from empty results). */
  apiKeys?: GQLAPIKey[];
}

/**
 * Retrieves a listing of active Pixie API keys, as well as functions to create and delete keys.
 * Creating a key requires no arguments; deleting one requires the ID of the key to be deleted.
 *
 * Usage:
 * ```
 * const [{ createAPIKey, deleteAPIKey, apiKeys }, loading, error] = useAPIKeys();
 * ```
 */
export function useAPIKeys(): ImmutablePixieQueryGuaranteedResult<APIKeyHookResult> {
  const { data, loading, error } = useQuery<{ apiKeys: GQLAPIKey[] }>(
    API_KEY_QUERIES.LIST_API_KEYS,
    { pollInterval: 2000 },
  );

  // Creation returns the ID of the created key and takes no arguments.
  const [creator] = useMutation<{ CreateAPIKey: { id: string } }, void>(API_KEY_QUERIES.CREATE_API_KEY);
  // Deletion returns a boolean stating whether it succeeded, and takes an ID to attempt to delete.
  const [deleter] = useMutation<void, { id: string }>(API_KEY_QUERIES.DELETE_API_KEY);

  // Waiting for state to settle ensures we don't change the result object's identity more than once.
  const resultIsStable = !loading && (error || data);

  // Abstracting away Apollo details, create resolves with the new ID string and delete accepts a string.
  // If an error occurs, it still falls through in the form of a rejected promise that .catch() will see.
  const result: APIKeyHookResult = React.useMemo(() => ({
    createAPIKey: () => creator().then(({ data: { CreateAPIKey: { id } } }) => id),
    deleteAPIKey: (id: string) => deleter({ variables: { id } }).then(() => {}),
    apiKeys: loading || error || !data?.apiKeys ? undefined : data.apiKeys,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [resultIsStable]);

  // Forcing the result to only present once it's settled, to avoid bouncy definitions.
  return [result, loading, error];
}
