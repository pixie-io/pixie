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

import { ApolloError } from '@apollo/client';
import {
  Autocomplete,
  AutocompleteRenderGetTagProps,
  Box,
  Button,
  Chip,
  FormControl,
  FormHelperText,
  Input,
  InputLabel,
  MenuItem,
  Paper,
  Select,
  Stack,
  TextField,
  Typography,
} from '@mui/material';
import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useHistory, useLocation } from 'react-router';

import { CodeEditor, EDITOR_THEME_MAP, StatusCell, StatusGroup, useSnackbar } from 'app/components';
import { usePluginConfig } from 'app/containers/admin/plugins/plugin-gql';
import { GQLClusterStatus, GQLEditableRetentionScript } from 'app/types/schema';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';
import { allowRetentionScriptName } from 'configurable/data-export';

import { PluginIcon } from './data-export-common';
import {
  ClusterInfoForRetentionScripts,
  DEFAULT_RETENTION_PXL,
  useClustersForRetentionScripts,
  useCreateRetentionScript,
  useMutateRetentionScript,
  useRetentionPlugins,
  useRetentionScript,
  useRetentionScripts,
} from './data-export-gql';

const useStyles = makeStyles(({ breakpoints, spacing, typography }: Theme) => createStyles({
  root: {
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'flex-start',
    alignItems: 'stretch',
    minHeight: '100%',
    maxHeight: '100%',
    paddingBottom: spacing(2),
    margin: `${spacing(2)} auto`,
    maxWidth: `${breakpoints.values.md}px`,
    width: '100%',

    '& > .MuiPaper-root': {
      padding: spacing(2),
      marginBottom: spacing(2),
    },
  },
  topControls: {
    flex: '0 0 auto',
    display: 'flex',
    justifyContent: 'space-between',
  },
  descriptionContainer: {
    marginTop: spacing(4),
    flex: '0 0 auto',
    display: 'flex',
    justifyContent: 'stretch',
    alignItems: 'stretch',

    '& > *': { flex: 1 },
  },
  scriptContainer: {
    flex: '1 1 auto',
    minHeight: spacing(50),
    display: 'flex',
    flexFlow: 'column nowrap',
    overflow: 'hidden',
  },
  scriptHeading: {
    ...typography.h3,
    marginBottom: spacing(2),
  },
  editorOuter: {
    flex: 1,
    minHeight: 0,
  },
  editorInner: {
    height: '100%',
    minHeight: spacing(40),
  },
  spinner: {
    position: 'absolute',
    top: '50%',
    left: '50%',
    transform: 'translate(-50%, -50%)',
  },
}), { name: 'EditDataExportScript' });

interface RetentionScriptEditorProps {
  initialValue: string;
  onChange: (value: string) => void;
  isReadOnly: boolean;
}
const RetentionScriptEditorInner = React.memo<RetentionScriptEditorProps>(({
  initialValue, onChange, isReadOnly = false,
}) => {
  const { width, height } = React.useContext(AutoSizerContext);
  const theme = useTheme();
  const classes = useStyles();
  const editorRef = React.createRef<CodeEditor>();

  React.useEffect(() => {
    if (!editorRef.current && !initialValue.trim().length) {
      return;
    }
    editorRef.current.changeEditorValue(initialValue);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [initialValue]);

  return (
    // Monaco calculates its own dimensions from its container, but it only succeeds if those dimensions are absolute.
    <div style={{ width, height }}>
      <CodeEditor
        ref={editorRef}
        onChange={onChange}
        className={classes.editorInner}
        spinnerClass={classes.spinner}
        visible={true}
        // eslint-disable-next-line react-memo/require-usememo
        shortcutKeys={[]}
        language='python'
        theme={EDITOR_THEME_MAP[theme.palette.mode]}
        isReadOnly={isReadOnly}
        readOnlyReason='PxL for preset scripts cannot be edited'
      />
    </div>
  );
});
RetentionScriptEditorInner.displayName = 'RetentionScriptEditor';

const RetentionScriptEditor = withAutoSizerContext(RetentionScriptEditorInner);

interface RetentionScriptForm {
  name: string;
  description: string;
  clusters: ClusterInfoForRetentionScripts[];
  contents: string;
  frequencyS: number;
  pluginID: string;
  exportPath: string;
}

function useCreateOrUpdateScript(scriptId: string, isCreate: boolean) {
  const create = useCreateRetentionScript();
  const update = useMutateRetentionScript(scriptId);
  return React.useMemo(() => (
    (!scriptId || isCreate) ? create : update
  ), [scriptId, isCreate, create, update]);
}

const ClusterOption = React.memo<{ props: any, cluster: ClusterInfoForRetentionScripts }>(({ props, cluster }) => {
  return (
    <li key={`option-${cluster.id}`} {...props}>
      <StatusCell statusGroup={cluster.status.replace('CS_', '').toLowerCase() as StatusGroup}/>
      {/* eslint-disable-next-line react-memo/require-usememo */}
      <Box sx={{ display: 'inline-block', ml: 1 }}>{cluster.prettyClusterName}</Box>
    </li>
  );
});
ClusterOption.displayName = 'ClusterOption';

const SelectedClusterChips = React.memo<{
  clusters: ClusterInfoForRetentionScripts[],
  getTagProps: AutocompleteRenderGetTagProps,
}>(({
  clusters, getTagProps,
}) => (
  /* eslint-disable react-memo/require-usememo */
  <>{clusters.map((cluster, index) => (
    <Chip key={`chip-${cluster.id}`} {...getTagProps({ index })} sx={{ mr: 1 }} label={(
      <Box sx={{ display: 'flex', flexFlow: 'row nowrap', alignItems: 'center' }}>
        <StatusCell statusGroup={cluster.status.replace('CS_', '').toLowerCase() as StatusGroup}/>
        <Box sx={{ ml: 1 }}>{cluster.prettyClusterName}</Box>
      </Box>
    )}/>
  ))}</>
  /* eslint-enable react-memo/require-usememo */
));
SelectedClusterChips.displayName = 'SelectedClusterChips';

export const EditDataExportScript = React.memo<{ scriptId: string, isCreate: boolean }>(({ scriptId, isCreate }) => {
  const classes = useStyles();
  const showSnackbar = useSnackbar();

  const history = useHistory();
  const navBackToAllScripts = React.useCallback(() => {
    history.push('/configure-data-export');
  }, [history]);

  const { clusters } = useClustersForRetentionScripts();
  const { plugins } = useRetentionPlugins();
  const { scripts: existingScripts } = useRetentionScripts();
  const { script } = useRetentionScript(scriptId);
  const { state } = useLocation();

  const takenScriptNames = React.useMemo(() => existingScripts.map(s => s.name), [existingScripts]);

  const enabledPlugins = React.useMemo(() => (plugins?.filter(p => p.retentionEnabled) ?? []), [plugins]);

  const createOrUpdate = useCreateOrUpdateScript(scriptId, isCreate);

  const validClusters = React.useMemo(() => {
    return clusters?.filter(
      c => c.status !== GQLClusterStatus.CS_DISCONNECTED || script?.clusters.includes(c.id),
    ) ?? [];
  }, [clusters, script?.clusters]);

  const [pendingValues, setFullPendingValues] = React.useState<RetentionScriptForm>({
    name: '',
    description: '',
    clusters: [],
    contents: (isCreate ? (state as any)?.contents || DEFAULT_RETENTION_PXL : ''),
    frequencyS: 10,
    pluginID: '',
    exportPath: '',
  });

  const { schema, values } = usePluginConfig(
    enabledPlugins.find(p => p.id === pendingValues.pluginID) ?? { id: '', enabledVersion: '', latestVersion: '' },
  );
  const allowCustomExportURL = schema?.allowCustomExportURL === true;
  const defaultExportURL = values?.customExportURL || schema?.defaultExportURL || '';

  const [dirty, setDirty] = React.useState(false);

  const setPendingField = React.useCallback(<K extends keyof RetentionScriptForm>(
    field: K,
    value: RetentionScriptForm[K],
  ) => {
    let newlyDirty = false;
    setFullPendingValues((prev) => {
      if (prev[field] === value) {
        return prev;
      }
      newlyDirty = true;
      return {
        ...prev,
        [field]: value,
      };
    });
    if (newlyDirty) setDirty(true);
  }, []);

  React.useEffect(() => {
    setDirty(false);
    setFullPendingValues({
      name: script?.name ?? '',
      description: script?.description ?? '',
      clusters: script?.clusters?.map(id => validClusters.find(c => c.id === id)).filter(c => c) ?? [],
      contents: script?.contents ?? (isCreate ? DEFAULT_RETENTION_PXL : ''),
      frequencyS: script?.frequencyS ?? 10,
      pluginID: script?.pluginID ?? '',
      exportPath: script?.customExportURL ?? '',
    });
  }, [isCreate, script, validClusters]);

  const [saving, setSaving] = React.useState(false);
  const valid = React.useMemo(() => (
    pendingValues.name.trim().length
      && (
        pendingValues.name.trim() === script?.name
        || (
          !takenScriptNames.includes(pendingValues.name.trim())
          && allowRetentionScriptName(pendingValues.name)
        )
      )
      && pendingValues.clusters != null
      && pendingValues.clusters.every(c => validClusters.some(v => v.id === c.id))
      && pendingValues.contents != null
      && pendingValues.frequencyS > 0
      && pendingValues.pluginID
      && enabledPlugins.some(p => p.id === pendingValues.pluginID)
      && pendingValues.exportPath != null
  ), [
    enabledPlugins, pendingValues.clusters, pendingValues.contents, pendingValues.exportPath, pendingValues.frequencyS,
    pendingValues.name, pendingValues.pluginID, takenScriptNames, validClusters, script?.name,
  ]);

  const nameErrorText = React.useMemo(() => {
    const name = pendingValues.name.trim();
    if (dirty && !name.length) {
      return 'Script needs a name';
    } else if (name !== script?.name && takenScriptNames.includes(name)) {
      return 'That name is already in use';
    } else if (!allowRetentionScriptName(name)) {
      return 'That name is not allowed';
    }
    return ' ';
  }, [pendingValues.name, dirty, script?.name, takenScriptNames]);

  const save = React.useCallback((e: React.FormEvent) => {
    e.preventDefault();
    e.stopPropagation();

    if (saving || !valid) return;
    setSaving(true);

    const newScript: GQLEditableRetentionScript = {
      name: pendingValues.name.trim(),
      description: pendingValues.description.trim(),
      frequencyS: pendingValues.frequencyS,
      enabled: script?.enabled ?? true,
      clusters: pendingValues.clusters.map(c => c.id),
      contents: pendingValues.contents,
      pluginID: pendingValues.pluginID,
      customExportURL: pendingValues.exportPath,
    };
    createOrUpdate(newScript)
      .then((res: string | boolean | ApolloError) => {
        setSaving(false);

        // Result is a string with the script's ID if created; `true` if it updated an existing one;
        // `false` if it failed for an unknown reason; and an ApolloError if it failed for a known reason.
        const created = (typeof res === 'string' && res.length);
        const updated = res === true;

        if (created || updated) {
          const message = created ? 'Script created!' : 'Retention script saved successfully';
          showSnackbar({ message });
          navBackToAllScripts();
        } else {
          const failureReason = (typeof res !== 'object' || !res) ? 'Unknown reason' : res.message;
          console.error('Failed to save retention script. Reason:', res);
          showSnackbar({ message: `Failed to save retention script! Reason: ${failureReason}` });
        }
      })
      .catch(() => setSaving(false));
  }, [
    saving, valid, pendingValues.name, pendingValues.description, pendingValues.frequencyS, pendingValues.clusters,
    pendingValues.contents, pendingValues.pluginID, pendingValues.exportPath, script?.enabled,
    createOrUpdate, showSnackbar, navBackToAllScripts,
  ]);

  const labelProps = React.useMemo(() => ({ shrink: true }), []);

  const fid = React.useId();

  return (
    /* eslint-disable react-memo/require-usememo */
    <form className={classes.root} onSubmit={save}>
      <Typography variant='h1' sx={{ mb: 2 }}>
        {isCreate ? 'Create Export Script' : (
          script?.isPreset ? 'Edit Preset Export Script' : 'Edit Custom Export Script'
        )}
      </Typography>
      <Paper>
        <div className={classes.topControls}>
          <TextField
            sx={{ width: '40ch' }}
            required
            error={nameErrorText !== ' '}
            helperText={nameErrorText}
            variant='standard'
            disabled={script?.isPreset}
            label='Script Name'
            value={pendingValues.name}
            onChange={(e) => setPendingField('name', e.target.value)}
            InputLabelProps={labelProps}
          />
          <Box sx={{ flex: 1 }} />
          <FormControl
            sx={{ width: '30ch' }}
            variant='standard'
            disabled={!isCreate}
            required={isCreate}
            error={!isCreate && !valid && !enabledPlugins.some(p => p.id === pendingValues.pluginID)}
          >
            <InputLabel htmlFor={`${fid}-plugin-id`}>
              {isCreate || script?.isPreset ? 'Plugin' : 'Plugin (set on creation)'}
            </InputLabel>
            <Select
              id={`${fid}-plugin-id`}
              value={pendingValues.pluginID}
              label='Plugin'
              onChange={(e) => setPendingField('pluginID', e.target.value)}
            >
              {enabledPlugins.map((plugin) => (
                <MenuItem key={plugin.id} value={plugin.id}>
                  <Box sx={{ display: 'flex', flexFlow: 'row nowrap', alignItems: 'center' }}>
                    <PluginIcon iconString={plugin.logo ?? ''} />
                    <span>{plugin.name}</span>
                  </Box>
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </div>
        <div className={classes.descriptionContainer}>
          <TextField
            multiline
            minRows={1}
            maxRows={10}
            variant='standard'
            disabled={script?.isPreset}
            label='Description'
            value={pendingValues.description}
            onChange={(e) => setPendingField('description', e.target.value)}
            InputLabelProps={labelProps}
          />
        </div>
      </Paper>
      <Paper>
        <Stack gap={4}>
          <Autocomplete
            sx={{ maxWidth: '100ch', minWidth: '40ch', width: 'max-content' }}
            multiple
            filterSelectedOptions
            options={validClusters}
            getOptionLabel={(c) => c.prettyClusterName}
            renderOption={(props, c) => <ClusterOption key={c.id} props={props} cluster={c} />}
            renderTags={(tagValue, getTagProps) => (
              <SelectedClusterChips clusters={tagValue} getTagProps={getTagProps} />
            )}
            value={pendingValues.clusters}
            onChange={(_, newVal) => (
              setPendingField('clusters', newVal as ClusterInfoForRetentionScripts[])
            )}
            renderInput={params => (
              <TextField
                {...params}
                variant='standard'
                label='Clusters'
                placeholder={pendingValues.clusters.length ? '' : 'All Clusters (Default)'}
                InputLabelProps={labelProps}
              />
            )}
          />
          <FormControl
            // eslint-disable-next-line react-memo/require-usememo
            sx={{ width: 'max-content' }}
            variant='standard'
            required
            error={!valid && !(pendingValues.frequencyS > 0)}
          >
            <InputLabel htmlFor={`${fid}-frequency-input`}>Summary Window (Seconds)</InputLabel>
            <Input
              id={`${fid}-frequency-input`}
              type='number'
              inputProps={{ min: 1, step: 1 }}
              value={pendingValues.frequencyS}
              onChange={(e) => setPendingField('frequencyS', +e.target.value)}
            />
            <FormHelperText>How frequently the script runs</FormHelperText>
          </FormControl>
          <TextField
            disabled={!allowCustomExportURL}
            required={allowCustomExportURL && !defaultExportURL}
            sx={{ width: '40ch' }}
            variant='standard'
            label='Export URL'
            placeholder={allowCustomExportURL
              ? (defaultExportURL ? `Default: ${defaultExportURL}` : 'Required.')
              : 'Not available with this plugin.'
            }
            value={pendingValues.exportPath}
            onChange={(e) => setPendingField('exportPath', e.target.value)}
            InputLabelProps={labelProps}
          />
        </Stack>
      </Paper>
      <Paper className={classes.scriptContainer}>
        <span className={classes.scriptHeading}>
          {script?.isPreset ? 'PxL Script (Read-Only)' : 'PxL Script'}
        </span>
        <div className={classes.editorOuter}>
          <RetentionScriptEditor
            initialValue={pendingValues.contents}
            onChange={(v) => setPendingField('contents', v)}
            isReadOnly={script?.isPreset}
          />
        </div>
      </Paper>
      <Box width={1} textAlign='right'>
        <Button variant='outlined' type='button' color='primary' onClick={navBackToAllScripts} sx={{ mr: 1 }}>
          Cancel
        </Button>
        <Button variant='contained' type='submit' disabled={saving || !valid}>
          {isCreate ? 'Create' : 'Save'}
        </Button>
      </Box>
      <Typography variant='caption' sx={{ color: 'error', textAlign: 'right', mt: 1 }}>
        {!valid ? 'Please check all required fields.' : <>&nbsp;</>}
      </Typography>
    </form>
    /* eslint-enable react-memo/require-usememo */
  );
});
EditDataExportScript.displayName = 'EditDataExportScript';
