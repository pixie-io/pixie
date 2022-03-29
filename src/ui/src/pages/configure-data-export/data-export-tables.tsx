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
  Add as AddIcon,
  Delete as DeleteIcon,
  Extension as ExtensionIcon,
  Settings as SettingsIcon,
} from '@mui/icons-material';
import {
  Box,
  Button,
  Divider,
  FormControlLabel,
  IconButton,
  Switch,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Tooltip,
  Typography,
} from '@mui/material';
import { styled } from '@mui/material/styles';
import { distanceInWordsStrict } from 'date-fns';

import { Spinner } from 'app/components';
import {
  GQLDetailedRetentionScript,
  GQLEditableRetentionScript,
  GQLPlugin,
  GQLRetentionScript,
} from 'app/types/schema';

import { GQL_GET_RETENTION_SCRIPT, GQL_GET_RETENTION_SCRIPTS, GQL_UPDATE_RETENTION_SCRIPT } from './data-export-gql';
import { getMockPluginsForRetentionScripts, getMockRetentionScripts, getMockRetentionScriptDetails } from './mock-data';


function useRetentionScripts(): { loading: boolean, scripts: GQLRetentionScript[] } {
  const { data, loading, error } = useQuery<GQLRetentionScript[]>(
    GQL_GET_RETENTION_SCRIPTS,
    {
      onError(err) {
        // TODO(nick,PC-1440): Snackbar error here?
        console.info('Could not fetch list of retention scripts:', err?.message);
      },
    },
  );

  // TODO(nick,PC-1440): Drop mock data
  const [scripts, setScripts] = React.useState<GQLRetentionScript[]>([]);
  React.useEffect(() => {
    if (data?.length) setScripts(data);
    else if (error?.message) setScripts(getMockRetentionScripts());
  }, [data, error]);

  return React.useMemo(() => ({
    loading: loading && !error,
    scripts,
  }), [scripts, error, loading]);
}

type PartialPlugin = Pick<GQLPlugin, 'id' | 'name' | 'description' | 'logo'>;
function useRetentionPlugins(): { loading: boolean, plugins: PartialPlugin[] } {
  const { data, loading, error } = useQuery<PartialPlugin[]>(
    gql`
      query GetRetentionPlugins {
        plugins(kind: PK_RETENTION) {
          id
          name
          description
          logo
        }
      }
    `,
    {
      onError(err) {
        // TODO(nick,PC-1440): Snackbar error here?
        console.info('Could not fetch list of plugins:', err?.message);
      },
    },
  );

  // TODO(nick,PC-1440): Drop mock data
  const [plugins, setPlugins] = React.useState<PartialPlugin[]>([]);
  React.useEffect(() => {
    if (data?.length) setPlugins(data);
    else if (error?.message) setPlugins(getMockPluginsForRetentionScripts());
  }, [data, error]);

  return React.useMemo(() => ({
    loading: loading && !error,
    plugins,
  }), [plugins, error, loading]);
}

// TODO(nick,PC-1440): Dedup <PluginIcon /> with Plugins page in Admin that already has a similar component
const PluginIcon = React.memo<{ iconString: string }>(({ iconString }) => {
  const looksValid = iconString?.includes('<svg');
  if (looksValid) {
    // Strip newlines and space that isn't required just in case
    const compacted = iconString.trim().replace(/\s+/gm, ' ').replace(/\s*([><])\s*/g, '$1');
    // fill="#fff", for instance, isn't safe without encoding the #.
    const dataUrl = `data:image/svg+xml;utf8,${encodeURIComponent(compacted)}`;
    const backgroundImage = `url("${dataUrl}")`;

    // eslint-disable-next-line react-memo/require-usememo
    return <Box sx={({ spacing }) => ({
      width: spacing(2.5),
      height: spacing(2.5),
      marginRight: spacing(1.5),
      background: backgroundImage ? `center/contain ${backgroundImage} no-repeat` : 'none',
    })} />;
  }

  // eslint-disable-next-line react-memo/require-usememo
  return <ExtensionIcon sx={{ mr: 2, fontSize: 'body1.fontSize' }} />;
});
PluginIcon.displayName = 'PluginIcon';

const RetentionScriptRow = React.memo<{ script: GQLRetentionScript, canDelete?: boolean }>(({
  script,
  canDelete = false,
}) => {
  const { plugins } = useRetentionPlugins();
  const { id, name, description, clusters, frequencyS, pluginID } = script;
  const plugin = plugins.find(p => p.id === pluginID);

  const { data: detailedData } = useQuery<GQLDetailedRetentionScript, { id: string }>(GQL_GET_RETENTION_SCRIPT, {
    variables: { id },
  });
  // TODO(nick,PC-1440): Drop mock data
  const detailedScript = React.useMemo(
    () => detailedData ?? getMockRetentionScriptDetails(script.id),
    [detailedData, script.id]);

  const [updateScript] = useMutation<{
    UpdateRetentionScript: boolean,
  }, {
    id: string,
    script: GQLEditableRetentionScript,
  }>(GQL_UPDATE_RETENTION_SCRIPT);

  const [saving, setSaving] = React.useState(false);
  const toggleScriptEnabled = React.useCallback((event: React.MouseEvent<HTMLLabelElement>) => {
    event.preventDefault();
    event.stopPropagation();

    if (!detailedScript) return;

    const edited: GQLEditableRetentionScript = {
      ...detailedScript,
      enabled: !detailedScript.enabled,
    };

    setSaving(true);
    updateScript({
      variables: {
        id: detailedScript.id,
        script: edited,
      },
      refetchQueries: [GQL_GET_RETENTION_SCRIPTS, GQL_GET_RETENTION_SCRIPT],
      onError(err) {
        // TODO(nick,PC-1440): Snackbar error here?
        console.info(`Could not toggle script ${detailedScript.id}:`, err?.message);
      },
    }).then(() => setSaving(false)).catch(() => setSaving(false));
  }, [detailedScript, updateScript]);

  return (
    /* eslint-disable react-memo/require-usememo */
    <TableRow key={id}>
      <TableCell>
        <Tooltip title={description}>
          <span>{name}</span>
        </Tooltip>
      </TableCell>
      <TableCell>
        {clusters.join(', ') || (
          <Typography variant='caption' sx={{ color: 'text.disabled' }}>All Clusters (Default)</Typography>
        )}
      </TableCell>
      <TableCell>
        <Tooltip title={frequencyS > 60 ? `${Number(frequencyS).toLocaleString()} seconds` : ''}>
          <span>{distanceInWordsStrict(0, frequencyS * 1000)}</span>
        </Tooltip>
      </TableCell>
      <TableCell>
        <Box sx={{ display: 'flex', flexFlow: 'row nowrap', alignItems: 'center' }}>
          <PluginIcon iconString={plugin?.logo ?? ''} />
          <span>{plugin?.name ?? ''}</span>
        </Box>
      </TableCell>
      <TableCell align='right' sx={({ spacing }) => ({ minWidth: spacing(33) })}>
        <FormControlLabel
          sx={{ mr: 1 }}
          label={script.enabled ? 'Enabled' : 'Disabled'}
          labelPlacement='start'
          onClick={toggleScriptEnabled}
          control={
            <Switch
              disabled={saving}
              checked={script.enabled}
            />
          }
        />
        <IconButton><SettingsIcon /></IconButton>
        {canDelete && <IconButton><DeleteIcon /></IconButton>}
      </TableCell>
    </TableRow>
    /* eslint-enable react-memo/require-usememo */
  );
});
RetentionScriptRow.displayName = 'RetentionScriptRow';

// eslint-disable-next-line react-memo/require-memo
const StyledTableHeaderCell = styled(TableCell)(({ theme }) => ({
  textTransform: 'uppercase',
  fontWeight: 300,
  color: theme.palette.foreground.three,
}));

const RetentionScriptTable = React.memo<{
  title: string, description: string, scripts: GQLRetentionScript[], isCustom?: boolean,
}>(({
  title, description, scripts, isCustom = false,
}) => {
  const canAddNewRow = isCustom;

  return (
    /* eslint-disable react-memo/require-usememo */
    <Box position='relative'>
      {canAddNewRow && (
        <Button
          size='small'
          variant='outlined'
          sx={({ spacing }) => ({ position: 'absolute', top: 0, right: spacing(2) })}
          startIcon={<AddIcon />}
        >
          Create Table
        </Button>
      )}
      <Typography variant='h2' ml={2} mb={2}>{title} Tables</Typography>
      {description.length > 0 && <Typography variant='subtitle2' ml={2} mb={4}>{description}</Typography>}
      {scripts.length > 0 ? (
        <Table>
          <TableHead>
            <TableRow>
              <StyledTableHeaderCell>Table Name</StyledTableHeaderCell>
              <StyledTableHeaderCell>Clusters</StyledTableHeaderCell>
              <StyledTableHeaderCell>Summary Window</StyledTableHeaderCell>
              <StyledTableHeaderCell>Export Location</StyledTableHeaderCell>
              <TableCell />
            </TableRow>
          </TableHead>
          <TableBody>
            {scripts.map(s => <RetentionScriptRow key={s.id} script={s} canDelete={isCustom} />)}
          </TableBody>
        </Table>
      ) : (
        <Typography variant='body1' ml={2}>No tables configured.</Typography>
      )}
    </Box>
    /* eslint-disable react-memo/require-usememo */
  );
});
RetentionScriptTable.displayName = 'RetentionScriptTable';


/*/
TODO(nick,PC-1440):
- Typography styles
  - Subtitle looks bad
  - Max width
  - Font size/weight/color overall
  - Ellipsis + tooltip on all columns
- Clusters column:
  - 'X more...' or '(+X)' badge that shows the whole list in line-break separated tooltip
  - Links? What would they go to though? /live/:clusterId ? Or just hover tooltips with the full ID or something.
- Summary Window column: reuse code from elsewhere to make the duration human-readable (300 seconds -> 5 minutes)
- Functionality for Create Table button
- Controls column:
  - Enable/disable switch
  - Configure button
  - "View in <Plugin Provider>" -- what is that?
- Configure page itself
  - Needs a back button to return to /configure-data-export
  - Needs a subroute - would `/configure-data-export/:scriptId` be unique? If not, /.../:pluginId/:scriptId maybe.
  - That needs a Monaco instance and some other stuff
  - Needs mutations of its own
  - Needs to change what it offers to set based on whether isPreset=true
  - Is a form below the script, has a save button like Plugins dropdown does
/*/

export const ConfigureDataExportBody = React.memo(() => {
  const { loading: loadingScripts, scripts } = useRetentionScripts();
  const { loading: loadingPlugins, plugins } = useRetentionPlugins();

  if (loadingScripts || loadingPlugins) {
    return (
      // eslint-disable-next-line react-memo/require-usememo
      <Box sx={{ height: 1, width: 1, display: 'flex', justifyContent: 'center', alignItems: 'center' }}>
        <Spinner />
      </Box>
    );
  }

  return (
    /* eslint-disable react-memo/require-usememo */
    <Box m={2} mt={4} mb={4}>
      {plugins.map(({ id, name, description }, i) => (
        <React.Fragment key={id}>
          {i > 0 && <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />}
          <RetentionScriptTable
            title={name}
            description={description}
            scripts={scripts.filter(s => s.pluginID === id && s.isPreset)}
          />
        </React.Fragment>
      ))}
      {plugins.length > 0 && <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />}
      <RetentionScriptTable
        title='Custom'
        description='Pixie can send results from custom scripts to long-term data stores at any desired frequency.'
        scripts={scripts.filter(s => !s.isPreset)}
        isCustom={true}
      />
    </Box>
    /* eslint-enable react-memo/require-usememo */
  );
});
ConfigureDataExportBody.displayName = 'ConfigureDataExportBody';
