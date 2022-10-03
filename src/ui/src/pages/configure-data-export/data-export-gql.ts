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

import { ApolloError, gql, useMutation, useQuery } from '@apollo/client';

import {
  GQLClusterStatus,
  GQLDetailedRetentionScript,
  GQLEditableRetentionScript,
  GQLPlugin,
  GQLRetentionScript,
} from 'app/types/schema';
import pixieAnalytics from 'app/utils/analytics';
import { customizeRetentionScript, customizeScriptList } from 'configurable/data-export';

export const DEFAULT_RETENTION_PXL = `import px

# Export your scripts to a long-term data store using OTLP.
# Query your dataframe as normal, but export to OTLP using \`px.export\`:
# px.export(
#   df, px.otel.Data(
#     resource={
#       # Service name is required.
#       'service.name': df.service,
#       # Optional specifications.
#       'k8s.container.name': df.container,
#       'service.instance.id': df.pod,
#       'k8s.pod.name': df.pod,
#       'k8s.namespace.name': df.namespace,
#       # We auto-include this value if it's not set.
#       # 'instrumentation.provider': df.pixie,
#     },
#     data=[
#       px.otel.metric.Gauge(
#         name='metric_name',
#         description=''
#         value=df.metric,
#         attributes={'attr': df.attr},
#       ),
#       px.otel.metric.Summary(
#         name='http.server.duration',
#         count=df.latency_count,
#         sum=df.latency_sum,
#         quantile_values={
#           0.0: df.latency_min,
#           1.0: df.latency_max,
#         },
#     )],
#   ),
# )
# Read more in https://docs.px.dev/tutorials/integrations/otel/.
`;

export const GQL_GET_PLUGINS_FOR_RETENTION_SCRIPTS = gql`
  query GetRetentionPlugins {
    plugins(kind: PK_RETENTION) {
      id
      name
      description
      logo
      retentionEnabled
      enabledVersion
      latestVersion
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

export const GQL_DELETE_RETENTION_SCRIPT = gql`
  mutation DeleteRetentionScript($id: ID!) {
    DeleteRetentionScript(id: $id)
  }
`;

export interface ClusterInfoForRetentionScripts {
  id: string;
  prettyClusterName: string;
  status?: GQLClusterStatus;
}

export function useClustersForRetentionScripts(): { loading: boolean, clusters: ClusterInfoForRetentionScripts[] } {
  const { data, loading, error } = useQuery<{ clusters: ClusterInfoForRetentionScripts[] }>(
    gql`
      query listClustersForRetentionScript {
        clusters {
          id
          prettyClusterName
          status
        }
      }
    `,
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    clusters: data?.clusters ?? [],
  }), [data, loading, error]);
}

export type PartialPlugin = Pick<
GQLPlugin,
'id' | 'name' | 'description' | 'logo' | 'retentionEnabled' | 'latestVersion' | 'enabledVersion'
>;

export function useRetentionPlugins(): { loading: boolean, error?: ApolloError, plugins: PartialPlugin[] } {
  const { data, loading, error } = useQuery<{ plugins: PartialPlugin[] }>(
    GQL_GET_PLUGINS_FOR_RETENTION_SCRIPTS,
    { fetchPolicy: 'cache-and-network' },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    error,
    plugins: data?.plugins ?? [],
  }), [data, loading, error]);
}

export function useRetentionScripts(): { loading: boolean, error?: ApolloError, scripts: GQLRetentionScript[] } {
  const { data, loading, error } = useQuery<{ retentionScripts: GQLRetentionScript[] }>(
    GQL_GET_RETENTION_SCRIPTS,
    { fetchPolicy: 'cache-and-network' },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    error,
    scripts: customizeScriptList(data?.retentionScripts ?? []),
  }), [data, loading, error]);
}

export function useRetentionScript(id: string): {
  loading: boolean,
  error?: ApolloError,
  script: GQLDetailedRetentionScript,
} {
  const { data, loading, error } = useQuery<{
    retentionScript: GQLDetailedRetentionScript,
  }, { id: string }>(
    GQL_GET_RETENTION_SCRIPT,
    {
      variables: { id },
      fetchPolicy: 'cache-and-network',
    },
  );

  return React.useMemo(() => ({
    loading: loading && !error,
    error,
    script: customizeRetentionScript(data?.retentionScript ?? null),
  }), [data, loading, error]);
}

export function useMutateRetentionScript(
  id: string,
): (newScript: GQLEditableRetentionScript) => Promise<boolean | ApolloError> {
  const { script } = useRetentionScript(id);

  const [updateScript] = useMutation<{
    UpdateRetentionScript: boolean,
  }, {
    id: string,
    script: GQLEditableRetentionScript,
  }>(GQL_UPDATE_RETENTION_SCRIPT);

  return React.useCallback((newScript: GQLEditableRetentionScript) => {
    if (!script) return Promise.reject('Not Ready');

    pixieAnalytics.track('Retention script modified', {
      name: newScript.name,
      frequencyS: newScript.frequencyS,
      clusters: newScript.clusters,
      plugin: newScript.pluginID,
      isPreset: script.isPreset,
      enabled: newScript.enabled,
    });

    return updateScript({
      variables: {
        id: script.id,
        script: {
          ...script,
          ...newScript,
          // TODO(michelle): When plugins can be changed on existing scripts, remove the `pluginID: undefined` part.
          pluginID: undefined,
          // Preset scripts have a few extra read-only fields; don't try to set them.
          ...(script.isPreset ? {
            name: undefined,
            description: undefined,
            contents: undefined,
          } : {}),
        },
      },
      refetchQueries: [GQL_GET_RETENTION_SCRIPTS, GQL_GET_RETENTION_SCRIPT],
      onError(err) {
        console.error(`Could not update script ${script.id}:`, err?.message);
      },
    }).then(
      ({ data: { UpdateRetentionScript: success } }) => success,
      (err) => err,
    );
  }, [script, updateScript]);
}

export function useToggleRetentionScript(id: string): (enabled?: boolean) => Promise<boolean | ApolloError> {
  const { script } = useRetentionScript(id);
  const mutate = useMutateRetentionScript(id);

  return React.useCallback((enabled?: boolean) => (
    mutate({
      ...script,
      enabled: enabled == null ? !script.enabled : enabled,
    })
  ), [mutate, script]);
}

export function useCreateRetentionScript(): (newScript: GQLEditableRetentionScript) => Promise<string | ApolloError> {
  const [createScript] = useMutation<{
    CreateRetentionScript: string,
  }, {
    script: GQLEditableRetentionScript,
  }>(GQL_CREATE_RETENTION_SCRIPT);

  return React.useCallback((newScript: GQLEditableRetentionScript) => {
    pixieAnalytics.track('Retention script created', {
      name: newScript.name,
      frequencyS: newScript.frequencyS,
      clusters: newScript.clusters,
      plugin: newScript.pluginID,
    });

    return createScript({
      variables: {
        script: newScript,
      },
      refetchQueries: [GQL_GET_RETENTION_SCRIPTS, GQL_GET_RETENTION_SCRIPT],
      onError(err) {
        console.error(`Could not create script named "${newScript.name}":`, err?.message);
      },
    }).then(
      (res) => {
        if (res.data) return res.data.CreateRetentionScript; // The ID of the script that was created
        else if (res.errors) return res.errors;
        throw new Error(`useCreateRetentionScript: response doesn't make sense: ${JSON.stringify(res)}`);
      },
      (err) => err,
    );
  }, [createScript]);
}

export function useDeleteRetentionScript(id: string): () => Promise<boolean | ApolloError> {
  const [deleteScript] = useMutation<{ DeleteRetentionScript: boolean }, { id: string }>(GQL_DELETE_RETENTION_SCRIPT);
  return React.useCallback(() => {
    pixieAnalytics.track('Retention script deleted', { id });

    return deleteScript({
      variables: { id },
      refetchQueries: [GQL_GET_RETENTION_SCRIPTS],
    }).then(
      ({ data: { DeleteRetentionScript: success } }) => success,
      (err) => err,
    );
  }, [id, deleteScript]);
}
