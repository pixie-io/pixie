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

import { Box, Button, Divider, Skeleton, Stack, TextField } from '@mui/material';

import { GQLPlugin } from 'app/types/schema';

import { usePluginConfig, usePluginConfigMutation } from './plugin-gql';

export const PluginConfig = React.memo<{ plugin: GQLPlugin }>(({ plugin }) => {
  const { loading, schema, values } = usePluginConfig(plugin);

  const [pendingValues, setPendingValues] = React.useState<Record<string, string>>({});

  const [saving, setSaving] = React.useState(false);
  const setDone = React.useCallback(() => {
    setSaving(false);
  }, []);
  const pushPluginConfig = usePluginConfigMutation(plugin);

  // TODO(nick,PC-1436): Race condition technically possible in save effect; make them cancellable
  const save = React.useCallback((e: React.FormEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setSaving(true);
    pushPluginConfig(
      Object.entries(pendingValues).map(([name, value]) => ({ name, value })),
    ).then(setDone).catch(setDone);
  }, [pendingValues, pushPluginConfig, setDone]);

  React.useEffect(() => {
    if (!values) return;
    setPendingValues((prev) => ({
      ...prev,
      ...values.reduce((accum, { name, value }) => ({ ...accum, [name]: value }), {}),
    }));
  }, [values]);

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
            helperText={pendingValues[name] ? description : ''}
            value={pendingValues[name] ?? ''}
            onChange={(e) => setPendingValues((prev) => ({ ...prev, [name]: e.target.value }))}
            InputLabelProps={{ shrink: true }} // Always put the label up top for consistency
          />
        ))}
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
