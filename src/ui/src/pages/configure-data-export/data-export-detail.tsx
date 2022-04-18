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
  Autocomplete,
  Box,
  Button,
  FormControl,
  Input,
  InputAdornment,
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
import { useHistory } from 'react-router';

import { CodeEditor, EDITOR_THEME_MAP, useSnackbar } from 'app/components';
import { usePluginConfig } from 'app/containers/admin/plugins/plugin-gql';
import { GQLClusterStatus, GQLEditableRetentionScript } from 'app/types/schema';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';

import {
  ClusterInfoForRetentionScripts,
  DEFAULT_RETENTION_PXL,
  PartialPlugin,
  useClustersForRetentionScripts,
  useCreateRetentionScript,
  useMutateRetentionScript,
  useRetentionPlugins,
  useRetentionScript,
} from './data-export-gql';

const useStyles = makeStyles(({ spacing, typography }: Theme) => createStyles({
  root: {
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'flex-start',
    alignItems: 'stretch',
    margin: spacing(2),
    paddingBottom: spacing(2),
    minHeight: '100%',
    maxHeight: '100%',

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

function useAllowCustomExportURL(plugin: PartialPlugin | null) {
  const { loading, schema } = usePluginConfig(plugin ?? { id: '', latestVersion: '' });
  return !loading && schema?.allowCustomExportURL === true;
}

export const EditDataExportScript = React.memo<{ scriptId: string, isCreate: boolean }>(({ scriptId, isCreate }) => {
  const classes = useStyles();
  const showSnackbar = useSnackbar();

  const history = useHistory();
  const backToTop = React.useCallback(() => {
    history.push('/configure-data-export');
  }, [history]);

  const { clusters } = useClustersForRetentionScripts();
  const { plugins } = useRetentionPlugins();
  const { script } = useRetentionScript(scriptId);

  const enabledPlugins = React.useMemo(() => (plugins?.filter(p => p.retentionEnabled) ?? []), [plugins]);

  const createOrUpdate = useCreateOrUpdateScript(scriptId, isCreate);

  const validClusters = React.useMemo(() => {
    return clusters.filter(
      c => c.status !== GQLClusterStatus.CS_DISCONNECTED,
    ) ?? [];
  }, [clusters]);

  const [pendingValues, setFullPendingValues] = React.useState<RetentionScriptForm>({
    name: '',
    description: '',
    clusters: [],
    contents: (isCreate ? DEFAULT_RETENTION_PXL : ''),
    frequencyS: 10,
    pluginID: '',
    exportPath: '',
  });

  const allowCustomExportURL = useAllowCustomExportURL(
    enabledPlugins.find(p => p.id === pendingValues.pluginID),
  );

  const setPendingField = React.useCallback(<K extends keyof RetentionScriptForm>(
    field: K,
    value: RetentionScriptForm[K],
  ) => {
    setFullPendingValues((prev) => ({
      ...prev,
      [field]: value,
    }));
  }, []);

  React.useEffect(() => {
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
  const [valid, setValid] = React.useState(true);
  const save = React.useCallback((e: React.FormEvent) => {
    e.preventDefault();
    e.stopPropagation();

    const nowValid = pendingValues.name.trim().length
      && pendingValues.clusters != null
      && pendingValues.clusters.every(c => validClusters.some(v => v.id === c.id))
      && pendingValues.contents != null
      && pendingValues.frequencyS > 0
      && pendingValues.pluginID
      && enabledPlugins.some(p => p.id === pendingValues.pluginID)
      && pendingValues.exportPath != null;
    setValid(nowValid);

    if (saving || !nowValid) return;
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
      .then((res: string | boolean) => {
        setSaving(false);
        let message = '';
        if (typeof res === 'boolean') {
          message = res ? 'Retention script saved successfully.' : 'Failed to save retention script, unknown reason.';
        } else {
          message = 'Script created!';
        }
        showSnackbar({
          message,
          actionTitle: 'Back to Scripts',
          action: backToTop,
        });
      })
      .catch(() => setSaving(false));
  }, [
    pendingValues.name, pendingValues.description, pendingValues.clusters, pendingValues.contents,
    pendingValues.frequencyS, pendingValues.pluginID, pendingValues.exportPath,
    enabledPlugins, saving, createOrUpdate, validClusters, script?.enabled, showSnackbar, backToTop,
  ]);

  const labelProps = React.useMemo(() => ({ shrink: true }), []);

  return (
    /* eslint-disable react-memo/require-usememo */
    <form className={classes.root} onSubmit={save}>
      <Paper className={classes.topControls}>
        <TextField
          required
          error={!valid && !pendingValues.name.trim().length}
          variant='standard'
          disabled={script?.isPreset}
          label='Script Name'
          value={pendingValues.name}
          onChange={(e) => setPendingField('name', e.target.value)}
          InputLabelProps={labelProps}
        />
        <Box sx={{ flex: 1 }} />
        <Autocomplete
          sx={{ flex: 1 }}
          multiple
          filterSelectedOptions
          options={validClusters}
          getOptionLabel={(c) => c.prettyClusterName}
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
      </Paper>
      <Paper className={classes.descriptionContainer}>
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
      </Paper>
      <Paper className={classes.scriptContainer}>
        <span className={classes.scriptHeading}>PxL Script</span>
        <div className={classes.editorOuter}>
          <RetentionScriptEditor
            initialValue={pendingValues.contents}
            onChange={(v) => setPendingField('contents', v)}
            isReadOnly={script?.isPreset}
          />
        </div>
      </Paper>
      <Paper>
        <Stack gap={2}>
          <FormControl
            // eslint-disable-next-line react-memo/require-usememo
            sx={{ width: '25ch' }}
            variant='standard'
            required
            error={!valid && !(pendingValues.frequencyS > 0)}
          >
            <InputLabel htmlFor='script-frequency-input'>Summary Window</InputLabel>
            <Input
              id='script-frequency-input'
              type='number'
              inputProps={{ min: 1, step: 1 }}
              value={pendingValues.frequencyS}
              onChange={(e) => setPendingField('frequencyS', +e.target.value)}
              endAdornment={
                // TODO(nick,PC-1440): Seconds/minutes/whatever selector here for convenience
                <InputAdornment position='end'>Seconds</InputAdornment>
              }
            />
          </FormControl>
          <FormControl
            sx={{ width: '25ch' }}
            variant='standard'
            disabled={!isCreate}
            required={isCreate}
            error={!isCreate && !valid && !enabledPlugins.some(p => p.id === pendingValues.pluginID)}
          >
            <InputLabel htmlFor='script-plugin-input'>Plugin</InputLabel>
            <Select
              id='script-plugin-input'
              value={pendingValues.pluginID}
              label='Plugin'
              onChange={(e) => setPendingField('pluginID', e.target.value)}
            >
              {enabledPlugins.map((plugin) => (
                <MenuItem key={plugin.id} value={plugin.id}>{plugin.name}</MenuItem>
              ))}
            </Select>
          </FormControl>
          <TextField
            disabled={!allowCustomExportURL}
            sx={{ width: '25ch' }}
            variant='standard'
            label='Export Path'
            placeholder={allowCustomExportURL
              ? 'Optional. Plugin-dependent.'
              : 'Not available with this plugin.'
            }
            value={pendingValues.exportPath}
            onChange={(e) => setPendingField('exportPath', e.target.value)}
            InputLabelProps={labelProps}
          />
        </Stack>
      </Paper>
      <Box width={1} textAlign='right'>
        <Button variant='outlined' type='button' color='primary' onClick={backToTop} sx={{ mr: 1 }}>
          Cancel
        </Button>
        <Button variant='contained' type='submit' disabled={saving || !valid} color={valid ? 'primary' : 'error'}>
          {isCreate ? 'Create' : 'Save'}
        </Button>
        {!valid && (
          <Typography variant='caption' sx={{ color: 'error' }}>Please fill in all required fields.</Typography>
        )}
      </Box>
    </form>
    /* eslint-enable react-memo/require-usememo */
  );
});
EditDataExportScript.displayName = 'EditDataExportScript';
