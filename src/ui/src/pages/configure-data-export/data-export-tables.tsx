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
  Add as AddIcon,
  Delete as DeleteIcon,
  Edit as EditIcon,
  History as HistoryIcon,
} from '@mui/icons-material';
import {
  Alert,
  Box,
  Button,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
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
import { Link, useRouteMatch } from 'react-router-dom';

import { isPixieEmbedded } from 'app/common/embed-context';
import { Spinner, useSnackbar } from 'app/components';
import { GQLRetentionScript } from 'app/types/schema';

import { PluginIcon } from './data-export-common';
import {
  useClustersForRetentionScripts,
  useDeleteRetentionScript,
  useRetentionPlugins,
  useRetentionScript,
  useRetentionScripts,
  useToggleRetentionScript,
} from './data-export-gql';

const RetentionScriptRow = React.memo<{ script: GQLRetentionScript }>(({ script }) => {
  const { path } = useRouteMatch();
  const { plugins } = useRetentionPlugins();
  const { id, name, description, clusters: selectedClusterIds, frequencyS, pluginID } = script;
  const plugin = plugins.find(p => p.id === pluginID);

  const { clusters: allClusters } = useClustersForRetentionScripts();
  const selectedClusters = React.useMemo(() => {
    return allClusters
      .filter(
        c => selectedClusterIds?.includes(c.id),
      ) ?? [];
  }, [allClusters, selectedClusterIds]);

  const { script: detailedScript } = useRetentionScript(id);

  const [saving, setSaving] = React.useState(false);
  const toggleMutation = useToggleRetentionScript(id);
  const toggleScriptEnabled = React.useCallback((event: React.SyntheticEvent) => {
    event.preventDefault();
    event.stopPropagation();

    if (!detailedScript) {
      console.info('Detailed script not set?');
      return;
    }
    setSaving(true);
    toggleMutation(!detailedScript.enabled)
      .then(() => setSaving(false))
      .catch(() => setSaving(false));
  }, [detailedScript, toggleMutation]);

  const showSnackbar = useSnackbar();
  const [promptingDelete, setPromptingDelete] = React.useState(false);
  const deleteMutation = useDeleteRetentionScript(id);
  const confirmDelete = React.useCallback(() => {
    setSaving(true);
    deleteMutation().then(
      (success) => {
        setSaving(false);
        if (success) {
          setPromptingDelete(false);
        } else {
          showSnackbar({
            message: `Failed to delete script "${name}", unknown reason`,
          });
        }
      },
      (err) => {
        setSaving(false);
        console.error(err);
        showSnackbar({
          message: `Failed to delete script "${name}", see console for details`,
        });
      },
    );
    setSaving(false);
  }, [deleteMutation, name, showSnackbar]);

  return (
    /* eslint-disable react-memo/require-usememo */
    <TableRow key={id}>
      <TableCell>
        <Tooltip title={description}>
          <Link to={`${path}/update/${script.id}`}>{name}</Link>
        </Tooltip>
      </TableCell>
      <TableCell>
        {selectedClusters.length > 0 ? (
          selectedClusters.map(cluster => (
            <Chip
              key={cluster.id}
              variant='outlined'
              size='small'
              label={cluster.prettyClusterName}
              sx={{ mr: 1 }}
            />
          ))
        ) : (
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
          onClick={(e) => toggleScriptEnabled(e)}
          control={
            <Switch
              disabled={saving}
              checked={script.enabled}
            />
          }
        />
        <Tooltip title='View execution logs'>
          <IconButton
            component={Link}
            to={`${path}/logs/${script.id}`}
          >
            <HistoryIcon />
          </IconButton>
        </Tooltip>
        <Tooltip title='Configure this script'>
          <IconButton
            component={Link}
            to={`${path}/update/${script.id}`}
          >
            <EditIcon />
          </IconButton>
        </Tooltip>
        {!script.isPreset && (
          <>
            <Tooltip title='Delete this script'>
              <IconButton onClick={() => setPromptingDelete(true)} disabled={saving}>
                <DeleteIcon />
              </IconButton>
            </Tooltip>
            <Dialog open={promptingDelete} onClose={() => setPromptingDelete(false)}>
              <DialogTitle>Delete Script</DialogTitle>
              <DialogContent>{`Delete script "${name}"? This cannot be undone.`}</DialogContent>
              <DialogActions>
                <Button onClick={() => setPromptingDelete(false)} variant='outlined' color='primary'>Cancel</Button>
                <Button onClick={confirmDelete} variant='contained' color='error'>Delete Script</Button>
              </DialogActions>
            </Dialog>
          </>
        )}
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

  const { path } = useRouteMatch();

  return (
    /* eslint-disable react-memo/require-usememo */
    <Box position='relative'>
      {canAddNewRow && (
        <Button
          size='small'
          variant='outlined'
          sx={({ spacing }) => ({ position: 'absolute', top: 0, right: spacing(2) })}
          startIcon={<AddIcon />}
          component={Link}
          to={`${path}/create`}
        >
          Create Script
        </Button>
      )}
      <Typography variant='h3' ml={2} mb={2}>{title}</Typography>
      {description.length > 0 && <Typography variant='subtitle2' ml={2} mb={4}>{description}</Typography>}
      {scripts.length > 0 ? (
        <Table>
          <TableHead>
            <TableRow>
              <StyledTableHeaderCell>Script Name</StyledTableHeaderCell>
              <StyledTableHeaderCell>Clusters</StyledTableHeaderCell>
              <StyledTableHeaderCell>Summary Window</StyledTableHeaderCell>
              <StyledTableHeaderCell>Export Location</StyledTableHeaderCell>
              <TableCell />
            </TableRow>
          </TableHead>
          <TableBody>
            {scripts.map(s => <RetentionScriptRow key={s.id} script={s} />)}
          </TableBody>
        </Table>
      ) : (
        <Typography variant='body1' ml={2}>No scripts configured.</Typography>
      )}
    </Box>
    /* eslint-disable react-memo/require-usememo */
  );
});
RetentionScriptTable.displayName = 'RetentionScriptTable';

export const ConfigureDataExportBody = React.memo(() => {
  const showSnackbar = useSnackbar();
  const isEmbedded = isPixieEmbedded();

  const { loading: loadingScripts, error: scriptsError, scripts } = useRetentionScripts();
  const { loading: loadingPlugins, error: pluginsError, plugins } = useRetentionPlugins();

  const enabledPlugins = React.useMemo(() => (plugins?.filter(p => p.retentionEnabled) ?? []), [plugins]);

  React.useEffect(() => {
    const pMsg = pluginsError?.message;
    const sMsg = scriptsError?.message;
    const msg = [pMsg && `Plugins: ${pMsg}`, sMsg && `Scripts: ${sMsg}`].filter(m => m).join('; ');
    if (msg) {
      console.error('Something went wrong while loading retention plugins and scripts...', msg);
      showSnackbar({ message: msg });
    }
  }, [pluginsError?.message, scriptsError?.message, showSnackbar]);

  if ((loadingScripts || loadingPlugins) && (!plugins || !scripts)) {
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
      <Typography variant='h1' ml={2} mb={2}>Data Retention Scripts</Typography>
      <Typography variant='body1' ml={2} mb={2}>
      {isEmbedded ? <></> : (
        <>
          {'These preset scripts are provided by your '}
          <Link to='/admin/plugins'>enabled plugins</Link>
          {'.'}
          <br /><br />
        </>
      )}
      {'You cannot edit the preset scripts, but you can change their arguments and which clusters they run on.'}
      <br />
      {'Write custom scripts by clicking Create Script at the bottom of the page. '}
      {'Learn more by visiting the '}
      <a href='https://docs.px.dev/tutorials/integrations/otel/#setup-the-plugin' target='_blank' rel='noreferrer'>
        plugin tutorial
      </a> and <a href='https://docs.px.dev/reference/plugins/plugin-system/' target='_blank' rel='noreferrer'>
        Pixie Plugin reference docs
      </a>.
      </Typography>
      <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />
      {(scriptsError || pluginsError) && (
        <Alert severity='error' variant='outlined' sx={{ ml: 2, mb: 2 }}>
          Something went wrong while loading retention plugins and scripts. See console for details.
        </Alert>
      )}
      {enabledPlugins.map(({ id, name, description }, i) => (
        <React.Fragment key={id}>
          {i > 0 && <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />}
          <RetentionScriptTable
            title={`Presets from ${name}`}
            description={description}
            scripts={scripts.filter(s => s.pluginID === id && s.isPreset).sort((a, b) => a.name.localeCompare(b.name))}
          />
        </React.Fragment>
      ))}
      {enabledPlugins.length > 0 && <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />}
      <RetentionScriptTable
        title='Custom Scripts'
        description='Pixie can send results from custom scripts to long-term data stores at any desired frequency.'
        scripts={scripts.filter(s => !s.isPreset).sort((a, b) => a.name.localeCompare(b.name))}
        isCustom={true}
      />
    </Box>
    /* eslint-enable react-memo/require-usememo */
  );
});
ConfigureDataExportBody.displayName = 'ConfigureDataExportBody';
