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
  Extension as ExtensionIcon,
  Settings as SettingsIcon,
} from '@mui/icons-material';
import {
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
import { styled, Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { distanceInWordsStrict } from 'date-fns';
import { Link, useRouteMatch } from 'react-router-dom';

import { Spinner, useSnackbar } from 'app/components';
import {
  GQLClusterStatus,
  GQLRetentionScript,
} from 'app/types/schema';

import {
  useClustersForRetentionScripts,
  useDeleteRetentionScript,
  useRetentionPlugins,
  useRetentionScript,
  useRetentionScripts,
  useToggleRetentionScript,
} from './data-export-gql';

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

const RetentionScriptRow = React.memo<{ script: GQLRetentionScript }>(({ script }) => {
  const { path } = useRouteMatch();
  const { plugins } = useRetentionPlugins();
  const { id, name, description, clusters: selectedClusterIds, frequencyS, pluginID } = script;
  const plugin = plugins.find(p => p.id === pluginID);

  const { clusters: allClusters } = useClustersForRetentionScripts();
  const selectedClusterNames = React.useMemo(() => {
    return allClusters
      .filter(
        c => c.status !== GQLClusterStatus.CS_DISCONNECTED && selectedClusterIds?.includes(c.id),
      ).map(
        c => c.prettyClusterName,
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
          <span>{name}</span>
        </Tooltip>
      </TableCell>
      <TableCell>
        {selectedClusterNames.length > 0 ? (
          selectedClusterNames.map(n => <Chip key={n} label={n} variant='outlined' size='small' />)
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
        <Tooltip title='Configure this script'>
          <IconButton
            component={Link}
            to={`${path}/update/${script.id}`}
          >
            <SettingsIcon />
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

const useStyles = makeStyles(({ palette }: Theme) => createStyles({
  link: {
    textDecoration: 'none',
    '&, &:visited': {
      color: palette.primary.main,
    },
    '&:hover': {
      textDecoration: 'underline',
    },
  },
}), { name: 'DataExport' });

export const ConfigureDataExportBody = React.memo(() => {
  const classes = useStyles();

  const { loading: loadingScripts, scripts } = useRetentionScripts();
  const { loading: loadingPlugins, plugins } = useRetentionPlugins();

  const enabledPlugins = React.useMemo(() => (plugins?.filter(p => p.retentionEnabled) ?? []), [plugins]);

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
        {'These scripts are provided by your '}
        <Link to='/admin/plugins' className={classes.link}>
          enabled plugins
        </Link>.
        They&apos;re enabled by default.<br/>
        Their PxL script can&apos;t be changed, but other options can.<br/>
        Custom scripts can be created at the bottom of this page.
      </Typography>
      <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />
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
