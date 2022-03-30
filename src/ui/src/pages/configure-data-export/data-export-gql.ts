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

import { gql, useMutation, useQuery } from '@apollo/client';

import {
  GQLDetailedRetentionScript,
  GQLEditableRetentionScript,
  GQLPlugin,
  GQLRetentionScript,
} from 'app/types/schema';

export const GQL_GET_PLUGINS_FOR_RETENTION_SCRIPTS = gql`
  query GetRetentionPlugins {
    plugins(kind: PK_RETENTION) {
      id
      name
      description
      logo
    }
  }
`;

export const GQL_GET_RETENTION_SCRIPTS = gql`
  query GetRetentionScripts {
    retentionScripts {
      id
      name
      description
      frequencyS
      enabled
      clusters
      pluginID
      isPreset
    }
  }
`;

export const GQL_GET_RETENTION_SCRIPT = gql`
  query GetRetentionScript($id: String!) {
    retentionScript(id: $id) {
      id
      name
      description
      frequencyS
      enabled
      clusters
      contents
      pluginID
      customExportURL
      isPreset
    }
  }
`;

export const GQL_UPDATE_RETENTION_SCRIPT = gql`
  mutation UpdateRetentionScript($id: ID!, $script: EditableRetentionScript) {
    UpdateRetentionScript(id: $id, script: $script)
  }
`;

export const GQL_CREATE_RETENTION_SCRIPT = gql`
  mutation CreateRetentionScript($script: EditableRetentionScript) {
    CreateRetentionScript(script: $script)
  }
`;

export type PartialPlugin = Pick<GQLPlugin, 'id' | 'name' | 'description' | 'logo'>;
export function useRetentionPlugins(): { loading: boolean, plugins: PartialPlugin[] } {
  const { data, loading, error } = useQuery<{ plugins: PartialPlugin[] }>(
    GQL_GET_PLUGINS_FOR_RETENTION_SCRIPTS,
    { fetchPolicy: 'cache-and-network' },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    plugins: data?.plugins ?? [],
  }), [data, loading, error]);
}

export function useRetentionScripts(): { loading: boolean, scripts: GQLRetentionScript[] } {
  const { data, loading, error } = useQuery<{ scripts: GQLRetentionScript[] }>(
    GQL_GET_RETENTION_SCRIPTS,
    { fetchPolicy: 'cache-and-network' },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    scripts: data?.scripts ?? [],
  }), [data, loading, error]);
}

export function useRetentionScript(id: string): { loading: boolean, script: GQLDetailedRetentionScript } {
  const { data, loading, error } = useQuery<GQLDetailedRetentionScript, { id: string }>(
    GQL_GET_RETENTION_SCRIPT,
    {
      variables: { id },
      fetchPolicy: 'cache-and-network',
    },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    script: data ?? null,
  }), [data, loading, error]);
}

export function useToggleRetentionScript(id: string): (enabled?: boolean) => Promise<boolean> {
  const { script } = useRetentionScript(id);

  const [updateScript] = useMutation<{
    UpdateRetentionScript: boolean,
  }, {
    id: string,
    script: GQLEditableRetentionScript,
  }>(GQL_UPDATE_RETENTION_SCRIPT);

  return React.useCallback((enabled?: boolean) => {
    if (!script) return Promise.reject('Not Ready');

    const edited: GQLEditableRetentionScript = {
      ...script,
      enabled: enabled == null ? !script.enabled : enabled,
    };

    return updateScript({
      variables: {
        id: script.id,
        script: edited,
      },
      refetchQueries: [GQL_GET_RETENTION_SCRIPTS, GQL_GET_RETENTION_SCRIPT],
      onError(err) {
        // TODO(nick,PC-1440): Snackbar error here?
        console.error(`Could not toggle script ${script.id}:`, err?.message);
      },
    }).then(({ data: { UpdateRetentionScript: success } }) => success);
  }, [script, updateScript]);
}
