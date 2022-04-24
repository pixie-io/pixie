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

import { Box, Button, Divider, FormControlLabel, Skeleton, Stack, Switch, TextField, Tooltip } from '@mui/material';

import { useSnackbar } from 'app/components';
import { GQLEditablePluginConfigs, GQLPlugin } from 'app/types/schema';

import { usePluginConfig, usePluginConfigMutation } from './plugin-gql';

export const PluginConfig = React.memo<{ plugin: GQLPlugin }>(({ plugin }) => {
  const { loading, schema, values } = usePluginConfig(plugin);
  const showSnackbar = useSnackbar();

  const [pendingValues, setPendingValues] = React.useState<GQLEditablePluginConfigs>({
    configs: [],
  });

  const [saving, setSaving] = React.useState(false);
  const pushPluginConfig = usePluginConfigMutation(plugin);

  // TODO(nick,PC-1436): Race condition technically possible in save effect; make them cancellable
  const save = React.useCallback((e: React.FormEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setSaving(true);
    pushPluginConfig({
      configs: [...pendingValues.configs],
      customExportURL: schema?.allowCustomExportURL ? pendingValues.customExportURL : undefined,
      insecureTLS: schema?.allowInsecureTLS ? pendingValues.insecureTLS : undefined,
    }).then((success) => {
      setSaving(false);
      if (success) {
        showSnackbar({ message: 'Changes saved', dismissible: true });
      } else {
        showSnackbar({ message: 'Failed to save changes!', dismissible: true });
      }
    }).catch((err) => {
      setSaving(false);
      showSnackbar({ message: 'Failed to save changes!', dismissible: true });
      console.error(`Failed to save changes for plugin ${plugin.name}:`, err);
    });
  }, [
    plugin.name,
    pushPluginConfig,
    schema?.allowCustomExportURL,
    schema?.allowInsecureTLS,
    pendingValues.configs,
    pendingValues.customExportURL,
    pendingValues.insecureTLS,
    showSnackbar,
  ]);

  React.useEffect(() => {
    if (!values) return;
    setPendingValues({
      configs: values.configs.map(({ name, value }) => ({ name, value })),
      customExportURL: values.customExportURL,
      insecureTLS: values.insecureTLS,
    });
  }, [values]);

  const insecureWarning = React.useMemo(() => (
    <>
      This plugin can be configured without TLS (Transport Layer Security).<br/>
      In most environments, disabling TLS is a <strong>Bad Idea&trade;</strong>.<br/>
      However, it can sometimes be useful to delay setting up TLS. This option exists for those scenarios.
    </>
  ), []);

  if (loading && (!schema || !values)) {
    return (
      /* eslint-disable react-memo/require-usememo */
      <Stack spacing={1}>
        <Skeleton animation='wave' sx={{ width: ({ spacing }) => spacing(25) }} />
        <Skeleton animation='wave' sx={{ width: ({ spacing }) => spacing(19) }} />
        <Skeleton animation='wave' sx={{ width: ({ spacing }) => spacing(22) }} />
      </Stack>
      /* eslint-enable react-memo/require-usememo */
    );
  }

  /* eslint-disable react-memo/require-usememo */
  return (
    <form onSubmit={save}>
      <Stack spacing={4}>
        {schema?.configs.map(({ name, description }) => (
          <TextField
            key={name}
            variant='outlined'
            label={name}
            placeholder={description}
            helperText={pendingValues.configs.find(c => c.name === name)?.value ? description : ''}
            value={pendingValues.configs.find(c => c.name === name)?.value ?? ''}
            onChange={(e) => setPendingValues((prev) => ({
              ...prev,
              configs: [
                ...prev.configs.filter(({ name: fname }) => fname !== name),
                { name, value: e.target.value },
              ],
            }))}
            InputLabelProps={{ shrink: true }} // Always put the label up top for consistency
          />
        ))}
        {schema?.allowCustomExportURL && (
          <TextField
            variant='outlined'
            label='Custom export path'
            placeholder='Default path for retention scripts'
            helperText={pendingValues.customExportURL ? 'Default path for retention scripts' : ''}
            value={pendingValues.customExportURL ?? ''}
            onChange={(e) => setPendingValues((prev) => ({ ...prev, customExportURL: e.target.value }))}
            InputLabelProps={{ shrink: true }}
          />
        )}
        {schema?.allowInsecureTLS && (
          <Tooltip arrow title={insecureWarning}>
            <FormControlLabel
              sx={{ width: 'fit-content' }}
              label='Secure connections with TLS'
              labelPlacement='end'
              onClick={() => setPendingValues((prev) => ({ ...prev, insecureTLS: !prev.insecureTLS }))}
              // eslint-disable-next-line react-memo/require-usememo
              control={
                <Switch size='small' checked={!pendingValues.insecureTLS} />
              }
            />
          </Tooltip>
        )}
      </Stack>
      <Divider variant='middle' sx={{ mt: 2, mb: 2 }} />
      {/* TODO(nick,PC-1436): Dedup code in the header's <MaterialSwitch />, maybe wrap form higher up */}
      <Box sx={{ display: 'flex', flexFlow: 'row nowrap', justifyContent: 'flex-end', alignItems: 'baseline' }}>
        <Button
          variant='contained'
          color='primary'
          sx={{ ml: 1 }}
          type='submit'
          disabled={saving}
        >
          Save
        </Button>
      </Box>
    </form>
    /* eslint-enable react-memo/require-usememo */
  );
});
PluginConfig.displayName = 'PluginConfig';
