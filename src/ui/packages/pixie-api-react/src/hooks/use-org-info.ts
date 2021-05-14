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
import { ORG_QUERIES, GQLOrgInfo } from '@pixie-labs/api';
import { ImmutablePixieQueryResult } from '../utils/types';

interface OrgInfoHookResult {
  updateOrgInfo: (id: string, enableApprovals: boolean) => Promise<void>;
  org: GQLOrgInfo;
}

export function useOrgInfo(): ImmutablePixieQueryResult<OrgInfoHookResult> {
  const { loading, data, error } = useQuery<{ org: GQLOrgInfo }>(
    ORG_QUERIES.GET_ORG_INFO,
    { pollInterval: 2000 },
  );

  const [updater] = useMutation<void, { id: string, enableApprovals: boolean }>(ORG_QUERIES.SET_ORG_INFO);

  // Waiting for state to settle ensures we don't change the result object's identity more than once.
  const resultIsStable = !loading && (error || data);

  // Abstracting away Apollo details, create resolves with the new ID string and delete accepts a string.
  // If an error occurs, it still falls through in the form of a rejected promise that .catch() will see.
  const result: OrgInfoHookResult = React.useMemo(() => ({
    updateOrgInfo: (id: string, enableApprovals: boolean) => updater({ variables: { id, enableApprovals } })
      .then(() => {}),
    org: loading || error || !data?.org ? undefined : data.org,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [resultIsStable]);

  // Forcing the result to only present once it's settled, to avoid bouncy definitions.
  return [result, loading, error];
}
