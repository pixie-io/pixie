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

import { ArrowBack, CheckCircle as SuccessIcon, Error as ErrorIcon } from '@mui/icons-material';
import { Alert, Autocomplete, Box, IconButton, Paper, TextField, Tooltip, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Link } from 'react-router-dom';

import { ClusterContext } from 'app/common/cluster-context';
import { Spinner, StatusCell, StatusGroup } from 'app/components';
import { DISPLAY_TYPE_KEY, TABLE_DISPLAY_TYPE } from 'app/containers/live/vis';
import { CompleteColumnDef } from 'app/containers/live-data-table/live-data-table';
import { QueryResultTable } from 'app/containers/live-widgets/table/query-result-viewer';
import { SetStateFunc } from 'app/context/common';
import { ResultsContext } from 'app/context/results-context';
import { GQLClusterStatus, GQLDetailedRetentionScript, GQLRetentionScript } from 'app/types/schema';

import { PluginIcon, useExportScriptStats } from './data-export-common';
import {
  ClusterInfoForRetentionScripts as Cluster,
  PartialPlugin,
  useClustersForRetentionScripts,
  useRetentionPlugins,
  useRetentionScript,
} from './data-export-gql';

const useStyles = makeStyles((theme: Theme) => createStyles({
  viewSpinnerContainer: {
    width: '100%',
    height: '100%',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'center',
    alignItems: 'center',
  },
  root: {
    padding: theme.spacing(2),
    minHeight: '100%',
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'flex-start',
    alignItems: 'stretch',
  },
  topBar: {
    width: '100%',
    flex: '0 0 auto',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing(2),
  },
  body: {
    flex: '1 0 auto',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'center',
    alignItems: 'center',
    overflow: 'auto',
  },
  paper: {
    flex: '1 0 auto',
    padding: theme.spacing(0.75),
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'stretch',

    '& > *': {
      flex: '1 1 auto',
    },
  },
  // Matching the one in canvas.tsx
  titlebar: {
    flex: '0 0 auto',
    backgroundColor: theme.palette.background.four,
    color: theme.palette.text.primary,
    height: theme.spacing(6),
    padding: theme.spacing(1.5),
    margin: theme.spacing(-0.75),
    marginBottom: 0,
    borderTopLeftRadius: 'inherit',
    borderTopRightRadius: 'inherit',
    borderBottom: `1px ${theme.palette.background.two} solid`,
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  title: {
    flex: '0 0 auto',
    fontSize: theme.spacing(2),
    fontWeight: 500,
    textTransform: 'capitalize',
  },
  titleInfix: {
    flex: '1 1 auto',
  },
  titleAffix: {
    flex: '0 0 auto',
  },
}), { name: 'DataExportHistory' });

const HistoryStatusIcon = React.memo<{
  cluster: Cluster,
  script: GQLRetentionScript,
}>(({ cluster, script }) => {
  const { loading, error, table } = useExportScriptStats(cluster?.id ?? '', script?.id ?? '');

  if (!cluster || !script || loading || error || !table) {
    return <StatusCell statusGroup='unknown' />;
  }

  let pass = 0;
  let fail = 0;
  for (const { Error:  rowError } of table.rows) {
    if (rowError) fail++;
    else pass++;
  }
  const pct = pass / (pass + fail);

  // eslint-disable-next-line react-memo/require-usememo
  let mode: StatusGroup = 'unknown';
  if (Number.isNaN(pct)) {
    mode = 'pending';
  } else {
    if (pct < 0.5) mode = 'unhealthy';
    else if (pct < 1) mode = 'degraded';
    else mode = 'healthy';
  }

  return <StatusCell statusGroup={mode} />;
});
HistoryStatusIcon.displayName = 'HistoryStatusIcon';


const HistoryClusterSelector = React.memo<{
  clusters: Cluster[],
  value: Cluster,
  onSelect: SetStateFunc<Cluster>,
  script: GQLRetentionScript,
}>(({ clusters, value, onSelect, script }) => {
  /* eslint-disable react-memo/require-usememo */
  return (
    <Autocomplete
      sx={{ width: '40ch' }}
      size='medium'
      options={clusters}
      value={value}
      onChange={(_, c: Cluster) => onSelect(c)}
      getOptionLabel={(c) => c.prettyClusterName}
      renderInput={(params) => (
        <TextField
          {...params}
          label='Cluster'
          placeholder='Select cluster...'
          InputProps={{
            ...params.InputProps,
            inputProps: {
              ...params.inputProps,
              form: 'no-op-form', // Prevents the Enter key from acting like pressing the Save button
              name: '', // And prevents this input from being part of the form's data
            },
            startAdornment: (
              <HistoryStatusIcon cluster={value} script={script} />
            ),
          }}
        />
      )}
      renderOption={(props, cluster) => (
        <li key={cluster.id} {...props}>
          <HistoryStatusIcon cluster={cluster} script={script} />
          <Box sx={{ display: 'inline-block', ml: 1 }}>{cluster.prettyClusterName}</Box>
        </li>
      )}
    />
  );
  /* eslint-enable react-memo/require-usememo */
});
HistoryClusterSelector.displayName = 'HistoryClusterSelector';

// A custom column added as a gutter on the left, to make it easier to tell if a run was successful.
// Just represents a success/fail based on whether the error message is an empty string or not.
const exportHistoryGutterCol: CompleteColumnDef = {
  id: 'history-error-gutter',
  accessor: 'Error', // Grabs the same data as the actual Error column on the right, and does something else with it.
  // eslint-disable-next-line react/display-name
  Cell({ value }) {
    const isError = value && String(value).trim().length > 0;
    return (
      <Tooltip title={isError ? 'This run was unsuccessful' : 'This run was successful'}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        { isError ? <ErrorIcon color='error' sx={{ mt: 1 }} /> : <SuccessIcon color='success' sx={{ mt: 1 }} />}
      </Tooltip>
    );
  },
};

export const DataExportHistory = React.memo<{
  script: GQLDetailedRetentionScript,
  plugin: PartialPlugin,
  clusters: Cluster[],
}>(({ script, plugin, clusters }) => {
  const classes = useStyles();

  const [cluster, setCluster] = React.useState<Cluster>(null);

  const { loading, error, table } = useExportScriptStats(cluster?.id, script.id);

  // LiveDataTable needs a fair bit of supporting structure even if we aren't using all of its features.
  const resultsCtx = React.useMemo(() => ({
    tables: new Map([[table?.name ?? 'temp', table]]),
    loading: false,
    setLoading: () => {},
    streaming: false,
    setStreaming: () => {},
    clearResults: () => {},
    setResults: () => {},
  }), [table]);

  const clusterCtx = React.useMemo(() => ({
    loading: false,
    selectedClusterID: cluster?.id,
    selectedClusterName: '',
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterStatus: cluster?.status,
    selectedClusterStatusMessage: '',
    selectedClusterUID: '',
    setClusterByName: () => {},
  }), [cluster]);

  const [titleAffix, setTitleAffix] = React.useState<React.ReactNode>(null);
  const titleAffixRef = React.useCallback((el: React.ReactNode) => { setTitleAffix(el); }, []);

  return (
    /* eslint-disable react-memo/require-usememo */
    <div className={classes.root}>
      <div className={classes.topBar}>
        <Tooltip title='Back to scripts'>
          <IconButton
            component={Link}
            to={'../'}
          >
            <ArrowBack />
          </IconButton>
        </Tooltip>
        <Typography variant='h1'>Export History</Typography>
        <Box sx={{ flexGrow: 1 }} />
        <TextField disabled variant='standard' label='Script' value={script.name} sx={{ mr: 2 }} />
        <TextField
          disabled variant='standard' label='Plugin' value={plugin.name}
          InputProps={{
            startAdornment: <PluginIcon iconString={plugin.logo ?? ''} />,
          }}
        />
        <Box sx={{ flexGrow: 1 }} />
        <HistoryClusterSelector clusters={clusters} value={cluster} onSelect={setCluster} script={script} />
      </div>
      {(!cluster || loading || error || !table?.rows.length) ? (
        <div className={classes.body}>
          {
            [
              !cluster && (
                <Typography variant='body1'>
                  {"Select a cluster to view this script's execution history on it."}
                </Typography>
              ),
              loading && <Spinner />,
              error && (
                <Alert severity='error' variant='outlined'>
                  <span>{error.message}!</span>
                  {[error.details].flat().map((d, i) => <p key={i}>{d}</p>)}
                </Alert>
              ),
              !table?.rows.length && (
                <Typography variant='body1'>
                  No results. Script has not been run on this cluster recently.
                </Typography>
              ),
            ].find(n => n) // Pick the first condition that matches (better than a mess of ternaries)
          }
        </div>
      ) : (
        <ResultsContext.Provider value={resultsCtx}>
          <ClusterContext.Provider value={clusterCtx}>
            <Paper className={classes.paper}>
              <div className={classes.titlebar}>
                <div className={classes.title}>Recent runs of script: {script.name}</div>
                <div className={classes.titleInfix}>{' '}</div>
                <div className={classes.titleAffix}>{titleAffix}</div>
              </div>
              <QueryResultTable
                display={{
                  [DISPLAY_TYPE_KEY]: TABLE_DISPLAY_TYPE,
                }}
                propagatedArgs={{}}
                table={table}
                customGutters={[exportHistoryGutterCol]}
                setExternalControls={titleAffixRef}
              />
            </Paper>
          </ClusterContext.Provider>
        </ResultsContext.Provider>
      )}
    </div>
    /* eslint-enable react-memo/require-usememo */
  );
});
DataExportHistory.displayName = 'DataExportHistory';

export const DataExportHistoryView = React.memo<{ scriptId: string }>(({ scriptId }) => {
  const classes = useStyles();

  const { clusters } = useClustersForRetentionScripts();
  const { plugins } = useRetentionPlugins();
  const { script } = useRetentionScript(scriptId);

  const validClusters = React.useMemo(() => {
    return clusters.filter(
      c => (
        (!script?.clusters.length && c.status !== GQLClusterStatus.CS_DISCONNECTED)
        || script?.clusters.includes(c.id)
      ),
    ) ?? [];
  }, [clusters, script?.clusters]);

  const plugin = React.useMemo(() => {
    if (!plugins?.length || !script?.pluginID) return null;
    return plugins.find((p) => p.id === script.pluginID);
  }, [script, plugins]);

  return (!plugin || !validClusters.length) ? (
    <div className={classes.viewSpinnerContainer}><Spinner /></div>
  ) : (
    <DataExportHistory clusters={validClusters} script={script} plugin={plugin} />
  );
});
DataExportHistoryView.displayName = 'DataExportHistoryView';
