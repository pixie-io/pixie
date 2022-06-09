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

import { GQL_GET_RETENTION_SCRIPTS } from 'app/pages/configure-data-export/data-export-gql';
import {
  GQLEditablePluginConfigs,
  GQLPlugin,
  GQLPluginInfo,
  GQLPluginKind,
  GQLRetentionPluginConfig,
} from 'app/types/schema';

/** The list of available plugins, their metadata, their versions, and their enablement states. */
export const GQL_GET_RETENTION_PLUGINS = gql`
  query GetRetentionPlugins {
    plugins (kind: PK_RETENTION) {
      name,
      id,
      description,
      logo,
      latestVersion,
      supportsRetention,
      retentionEnabled,
      enabledVersion,
    }
  }
`;

/** The configuration schema for a given plugin (which configs can exist and their descriptions) */
export const GQL_GET_RETENTION_PLUGIN_INFO = gql`
  query GetRetentionPluginInfo($id: String!, $pluginVersion: String!) {
    retentionPluginInfo (id: $id, pluginVersion: $pluginVersion) {
      configs {
        name
        description
      }
      allowCustomExportURL
      defaultExportURL
      allowInsecureTLS
    }
  }
`;

/** The configuration values for a given plugin (see retentionPluginInfo for corresponding schema) */
export const GQL_GET_RETENTION_PLUGIN_CONFIG = gql`
  query GetRetentionPluginConfig($id: String!) {
    retentionPluginConfig(id: $id) {
      configs {
        name
        value
      }
      customExportURL
      insecureTLS
    }
  }
`;

export const GQL_UPDATE_RETENTION_PLUGIN_CONFIG = gql`
  mutation UpdateRetentionPluginConfig(
    $id: String!,
    $enabled: Boolean,
    $enabledVersion: String,
    $configs: EditablePluginConfigs!,
  ) {
    UpdateRetentionPluginConfig(
      id: $id,
      enabled: $enabled,
      enabledVersion: $enabledVersion,
      configs: $configs,
    )
  }
`;

export function usePluginList(kind: GQLPluginKind): {
  plugins: GQLPlugin[],
  loading: boolean,
  error?: ApolloError,
} {
  if (kind !== GQLPluginKind.PK_RETENTION) {
    throw new Error(`Plugins of kind "${GQLPluginKind[kind]}" aren't supported yet`);
  }

  const { data, loading, error } = useQuery<{ plugins: GQLPlugin[] }>(
    GQL_GET_RETENTION_PLUGINS,
    { fetchPolicy: 'cache-and-network' },
  );

  // Memo so that the empty array retains its identity until there is an actual change
  const plugins = React.useMemo(() => data?.plugins ?? [], [data?.plugins]);
  return {
    plugins,
    loading: loading || (!data && !error),
    error,
  };
}

export function usePluginConfig(plugin: Pick<GQLPlugin, 'id' | 'enabledVersion' | 'latestVersion'>): {
  schema: GQLPluginInfo,
  values: GQLRetentionPluginConfig,
  loading: boolean,
  valuesError?: ApolloError,
  schemaError?: ApolloError,
} {
  const { loading: loadingSchema, data: schemaData, error: schemaError } = useQuery<{
    retentionPluginInfo: GQLPluginInfo,
  }, {
    id: string, pluginVersion: string,
  }>(
    GQL_GET_RETENTION_PLUGIN_INFO,
    {
      variables: { id: plugin.id, pluginVersion: plugin.enabledVersion ?? plugin.latestVersion },
      fetchPolicy: 'cache-and-network',
    },
  );

  const { loading: loadingValues, data: valuesData, error: valuesError } = useQuery<{
    retentionPluginConfig: GQLRetentionPluginConfig,
  }, {
    id: string,
  }>(
    GQL_GET_RETENTION_PLUGIN_CONFIG,
    {
      variables: { id: plugin.id },
      fetchPolicy: 'no-cache', // Configuration values likely include secrets, so don't cache them.
    },
  );

  return React.useMemo(() => ({
    schema: schemaData?.retentionPluginInfo ?? null,
    values: valuesData?.retentionPluginConfig ?? null,
    loading: (loadingSchema || loadingValues) && !(schemaError || valuesError),
    schemaError,
    valuesError,
  }), [loadingSchema, loadingValues, schemaError, valuesError, schemaData, valuesData]);
}

export function usePluginConfigMutation(plugin: GQLPlugin): (
  configs: GQLEditablePluginConfigs,
  enabled?: boolean,
) => Promise<ApolloError | null> {
  const { loading, schema, values: oldConfigs } = usePluginConfig(plugin);

  const [mutate] = useMutation<{
    UpdateRetentionPluginConfig: boolean
  }, {
    id: string, enabled: boolean, enabledVersion: string, configs: GQLEditablePluginConfigs,
  }>(GQL_UPDATE_RETENTION_PLUGIN_CONFIG);

  return React.useCallback((newConfigs: GQLEditablePluginConfigs, enabled) => {
    if (loading) return Promise.reject('Tried to update a plugin config before its previous config was known');

    const allowed = schema?.configs.map(s => s.name) ?? [];
    const merged = oldConfigs?.configs.map(c => ({ ...c })) ?? [];
    for (const nc of newConfigs.configs) {
      if (!allowed.includes(nc.name)) continue;
      const existing = merged.findIndex(oc => oc.name === nc.name);
      if (existing >= 0) {
        merged[existing].value = nc.value;
      } else {
        merged.push({ ...nc });
      }
    }

    const customExportURL = (schema?.allowCustomExportURL
      ? (newConfigs.customExportURL ?? oldConfigs?.customExportURL)
      : null
    ) ?? undefined;

    const insecureTLS = (schema?.allowInsecureTLS
      ? (newConfigs.insecureTLS ?? oldConfigs?.insecureTLS)
      : null
    ) ?? undefined;

    const canEnable = plugin.supportsRetention && (plugin.enabledVersion ?? plugin.latestVersion);
    const shouldEnable = canEnable && (enabled == null ? plugin.retentionEnabled : enabled);

    return mutate({
      variables: {
        id: plugin.id,
        enabled: shouldEnable,
        enabledVersion: shouldEnable ? (plugin.enabledVersion ?? plugin.latestVersion) : plugin.enabledVersion,
        configs: {
          configs: merged,
          customExportURL,
          insecureTLS,
        },
      },
      refetchQueries: [GQL_GET_RETENTION_PLUGINS, GQL_GET_RETENTION_PLUGIN_CONFIG, GQL_GET_RETENTION_SCRIPTS],
      onError(err) {
        console.error(`Could not update plugin ${plugin.id}:`, err?.message);
      },
    }).then(
      ({ data: { UpdateRetentionPluginConfig: success } }) => success,
      (err) => err,
    );
  }, [schema?.configs, schema?.allowCustomExportURL, schema?.allowInsecureTLS, oldConfigs, plugin, loading, mutate]);
}

export function usePluginToggleEnabled(plugin: GQLPlugin): (enable: boolean) => Promise<ApolloError | null> {
  const mutate = usePluginConfigMutation(plugin);

  return (enable: boolean) => mutate({ configs: [] }, enable);
}
