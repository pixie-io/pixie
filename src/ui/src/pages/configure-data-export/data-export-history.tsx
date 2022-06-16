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

import { ArrowBack } from '@mui/icons-material';
import { Alert, Autocomplete, Box, IconButton, Paper, TextField, Tooltip, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Link } from 'react-router-dom';

import { ClusterContext } from 'app/common/cluster-context';
import { Spinner, StatusCell, StatusGroup } from 'app/components';
import { QueryResultTable } from 'app/containers/live-widgets/table/query-result-viewer';
import { DISPLAY_TYPE_KEY, TABLE_DISPLAY_TYPE } from 'app/containers/live/vis';
import { SetStateFunc } from 'app/context/common';
import { ResultsContext } from 'app/context/results-context';
import { GQLClusterStatus, GQLDetailedRetentionScript } from 'app/types/schema';

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
    padding: theme.spacing(1),
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'stretch',
  },
  tableTitle: {
    ...theme.typography.h3,
    padding: theme.spacing(0.5),
    color: theme.palette.foreground.two,
    textTransform: 'capitalize',
    marginBottom: theme.spacing(1.8),
  },
  tableWrap: {
    flex: '1 1 100%',
    position: 'relative',
  },
}), { name: 'DataExportHistory' });

const HistoryClusterSelector = React.memo<{
  clusters: Cluster[],
  value: Cluster,
  onSelect: SetStateFunc<Cluster>,
}>(({ clusters, value, onSelect }) => {
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
              <StatusCell statusGroup={
                (value?.status.replace('CS_', '').toLowerCase() ?? 'unknown') as StatusGroup
              } />
            ),
          }}
        />
      )}
      renderOption={(props, cluster) => (
        <li key={cluster.id} {...props}>
          <StatusCell statusGroup={cluster.status.replace('CS_', '').toLowerCase() as StatusGroup} />
          <Box sx={{ display: 'inline-block', ml: 1 }}>{cluster.prettyClusterName}</Box>
        </li>
      )}
    />
  );
  /* eslint-enable react-memo/require-usememo */
});
HistoryClusterSelector.displayName = 'HistoryClusterSelector';

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
    selectedClusterVizierConfig: null,
    setClusterByName: () => {},
  }), [cluster]);

  return (
    /* eslint-disable react-memo/require-usememo */
    <div className={classes.root}>
      <div className={classes.topBar}>
        <Tooltip title='Back to scripts'>
          <IconButton
            component={Link}
            to={'/configure-data-export'}
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
        <HistoryClusterSelector clusters={clusters} value={cluster} onSelect={setCluster} />
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
              <h3 className={classes.tableTitle}>Recent runs of script: {script.name}</h3>
              <div className={classes.tableWrap}>
                <div style={{ display: 'block', position: 'absolute', height: '100%', width: '100%' }}>
                  <QueryResultTable
                    display={{
                      [DISPLAY_TYPE_KEY]: TABLE_DISPLAY_TYPE,
                    }}
                    propagatedArgs={{}}
                    table={table}
                  />
                </div>
              </div>
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
      c => c.status !== GQLClusterStatus.CS_DISCONNECTED || script?.clusters.includes(c.id),
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
