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
  GQLEditablePluginConfig,
  GQLEditablePluginConfigs,
  GQLPlugin,
  GQLPluginConfig,
  GQLPluginInfo,
} from 'app/types/schema';

import { getMockSchemaFor, getMockDefaultConfigFor, useApolloCacheForMock } from './mock-data';

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
        name,
        description,
      },
    }
  }
`;

/** The configuration values for a given plugin (see retentionPluginInfo for corresponding schema) */
export const GQL_GET_RETENTION_PLUGIN_CONFIG = gql`
  query GetRetentionPluginConfig($id: String!) {
    orgRetentionPluginConfig(id: $id) {
      name,
      value,
    }
  }
`;

export const GQL_UPDATE_RETENTION_PLUGIN_CONFIG = gql`
  mutation UpdateRetentionPluginConfig(
    $id: String!, $enabled: Boolean, $enabledVersion: String, $configs: EditablePluginConfigs!
  ) {
    UpdateRetentionPluginConfig(id: $id, enabled: $enabled, enabledVersion: $enabledVersion, configs: $configs)
  }
`;

export function usePluginConfig(plugin: GQLPlugin): {
  loading: boolean,
  schema: GQLPluginInfo,
  values: GQLPluginConfig[]
} {
  const apolloCache = useApolloCacheForMock();

  const { loading: loadingSchema, data: schemaData, error: schemaError } = useQuery<{
    retentionPluginInfo: GQLPluginInfo,
  }, {
    id: string, pluginVersion: string,
  }>(
    GQL_GET_RETENTION_PLUGIN_INFO,
    {
      variables: { id: plugin.id, pluginVersion: plugin.enabledVersion ?? plugin.latestVersion },
      errorPolicy: 'all',
      onError(err) {
        if (err?.message.includes('NotFound')) {
          // TODO(nick,PC-1436): Instead of using mock data, tell the user that the plugin's schema is missing.
          const mock = getMockSchemaFor(plugin.id);
          if (mock) {
            // TODO(nick,PC-1436): Drop debugging console
            console.warn(`MOCK DATA! "${plugin.id}", is NotFound upstream but is mocked.`);
            apolloCache.writeQuery({
              query: GQL_GET_RETENTION_PLUGIN_INFO,
              variables: { id: plugin.id, pluginVersion: plugin.enabledVersion ?? plugin.latestVersion },
              data: { retentionPluginInfo: mock },
            });
          }
        }
      },
    },
  );

  const { loading: loadingValues, data: valuesData, error: valuesError } = useQuery<{
    orgRetentionPluginConfig: GQLPluginConfig[],
  }, {
    id: string,
  }>(
    GQL_GET_RETENTION_PLUGIN_CONFIG,
    {
      variables: { id: plugin.id },
      onError(err) {
        if (err?.message.includes('Unimplemented')) {
          // TODO(nick,PC-1436): Instead of using mock data, tell the user that plugin config couldn't be fetched.
          const mock = getMockDefaultConfigFor(plugin.id);
          if (mock) {
            // TODO(nick,PC-1436): Drop debugging console
            console.warn(`MOCK DATA! "${plugin.id}", GetRetentionPluginConfig Unimplemented, but is mocked.`);
            apolloCache.writeQuery({
              query: GQL_GET_RETENTION_PLUGIN_CONFIG,
              variables: { id: plugin.id },
              data: { orgRetentionPluginConfig: mock },
            });
          }
        }
      },
    },
  );

  // TODO(nick,PC-1436): Mock data, breaks mutations (since the cache gets clobbered repeatedly)
  const schema: GQLPluginInfo = React.useMemo(() => (
    schemaError?.message.includes('NotFound') ?
      getMockSchemaFor(plugin.id) : schemaData?.retentionPluginInfo
  ), [plugin.id, schemaData?.retentionPluginInfo, schemaError?.message]);
  const values: GQLPluginConfig[] = React.useMemo(() => (
    valuesError?.message.includes('Unimplemented') ?
      getMockDefaultConfigFor(plugin.id) : valuesData?.orgRetentionPluginConfig
  ), [plugin.id, valuesData?.orgRetentionPluginConfig, valuesError?.message]);

  return React.useMemo(() => ({
    loading: (loadingSchema || loadingValues) && !(schemaError || valuesError),
    schema: schemaData?.retentionPluginInfo ?? schema ?? null,
    values: valuesData?.orgRetentionPluginConfig ?? values ?? null,
  }), [loadingSchema, loadingValues, schemaError, valuesError, schemaData, valuesData, schema, values]);
}

export function usePluginConfigMutation(plugin: GQLPlugin): (configs: GQLEditablePluginConfig[]) => Promise<boolean> {
  const { loading, schema, values: oldConfigs } = usePluginConfig(plugin);

  const [mutate] = useMutation<{
    UpdateRetentionPluginConfig: boolean
  }, {
    id: string, enabled: boolean, enabledVersion: string, configs: GQLEditablePluginConfigs,
  }>(GQL_UPDATE_RETENTION_PLUGIN_CONFIG);

  return React.useCallback((newConfigs: GQLEditablePluginConfig[]) => {
    if (loading) return Promise.reject('Tried to update a plugin config before its previous config was known');

    // TODO(nick,PC-1436): Drop debugging console
    console.info(`usePluginConfigMutation(${plugin.id}) -> save(${JSON.stringify(newConfigs)})`);

    const allowed = schema.configs.map(s => s.name);
    const merged = oldConfigs.map(c => ({ ...c }));
    for (const nc of newConfigs) {
      if (!allowed.includes(nc.name)) continue;
      const existing = merged.findIndex(oc => oc.name === nc.name);
      if (existing >= 0) {
        merged[existing].value = nc.value;
      } else {
        merged.push({ ...nc });
      }
    }

    return mutate({
      variables: {
        id: plugin.id,
        enabled: plugin.supportsRetention && plugin.enabledVersion && plugin.retentionEnabled,
        enabledVersion: plugin.enabledVersion,
        configs: { configs: merged },
      },
    }).then(() => true, () => false);
  }, [
    loading, schema?.configs, oldConfigs, mutate,
    plugin.id, plugin.supportsRetention, plugin.enabledVersion, plugin.retentionEnabled,
  ]);
}

export function usePluginToggleEnabled(plugin: GQLPlugin): (enable: boolean) => Promise<boolean> {
  const [mutate] = useMutation<{
    UpdateRetentionPluginConfig: boolean
  }, {
    id: string, enabled: boolean, enabledVersion: string, configs: GQLEditablePluginConfigs,
  }>(GQL_UPDATE_RETENTION_PLUGIN_CONFIG);

  const apolloCache = useApolloCacheForMock();
  return React.useCallback((enable: boolean) => {

    // TODO(nick,PC-1436): Drop debugging console
    console.info(`usePluginToggleEnabled(${plugin.id}) -> toggle(enable=${enable})`);

    return mutate({
      variables: {
        id: plugin.id,
        enabled: enable,
        enabledVersion: plugin.enabledVersion ?? plugin.latestVersion,
        configs: { configs: [] },
      },
      refetchQueries: [GQL_GET_RETENTION_PLUGINS],
      onError(err) {
        if (err?.message?.includes('Unimplemented')) {
          // TODO(nick,PC-1436): Make this an `update` function instead once mock data is removed.
          // TODO(nick,PC-1436): Drop debugging console
          console.warn('MOCK DATA! Toggling cached enable state of plugin directly because mutation is NYI upstream.');
          apolloCache.modify({
            fields: {
              plugins(existingPlugins) {
                return existingPlugins.map((p) => {
                  return {
                    ...p,
                    retentionEnabled: (p.id === plugin.id ? !plugin.retentionEnabled : p.retentionEnabled),
                  };
                });
              },
            },
          });
        }
      },
      optimisticResponse: { UpdateRetentionPluginConfig: true },
    }).then(({ data: { UpdateRetentionPluginConfig } }) => UpdateRetentionPluginConfig);
  }, [apolloCache, mutate, plugin.enabledVersion, plugin.id, plugin.latestVersion, plugin.retentionEnabled]);
}
