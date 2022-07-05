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

import { Extension as ExtensionIcon } from '@mui/icons-material';
import { Box } from '@mui/material';
import { useTheme } from '@mui/material/styles';

import { ExecutionStateUpdate, PixieAPIClientAbstract, PixieAPIContext, VizierQueryError, VizierTable } from 'app/api';
import { PASSTHROUGH_PROXY_PORT } from 'app/containers/constants';
import { DISPLAY_TYPE_KEY, getQueryFuncs, TABLE_DISPLAY_TYPE, Vis } from 'app/containers/live/vis';
import { GQLClusterStatus, GQLRetentionScript } from 'app/types/schema';
import { CancellablePromise, makeCancellable } from 'app/utils/cancellable-promise';
import { checkExhaustive } from 'app/utils/check-exhaustive';
import { WithChildren } from 'app/utils/react-boilerplate';

import { useClustersForRetentionScripts } from './data-export-gql';

export const PluginIcon = React.memo<{ iconString: string }>(({ iconString }) => {
  const { spacing } = useTheme();
  const looksValid = iconString?.includes('<svg');
  if (looksValid) {
    // Strip newlines and space that isn't required just in case
    const compacted = iconString.trim().replace(/\s+/gm, ' ').replace(/\s*([><])\s*/g, '$1');
    // fill="#fff", for instance, isn't safe without encoding the #.
    const dataUrl = `data:image/svg+xml;utf8,${encodeURIComponent(compacted)}`;
    const backgroundImage = `url("${dataUrl}")`;

    // eslint-disable-next-line react-memo/require-usememo
    return <Box sx={{
      width: spacing(2.5),
      height: spacing(2.5),
      marginRight: spacing(1.5),
      background: backgroundImage ? `center/contain ${backgroundImage} no-repeat` : 'none',
    }} />;
  }

  // eslint-disable-next-line react-memo/require-usememo
  return <ExtensionIcon sx={{ mr: 2, fontSize: 'body1.fontSize' }} />;
});
PluginIcon.displayName = 'PluginIcon';

function getStatsScript(scriptId?: string): { pxl: string, vis: Vis, args: Record<string, string> } {
  const pxl = `
import px

def cron_stats(script_id: str):
  df = px.GetCronScriptHistory()

  filters = (script_id == '' or script_id == df.script_id)
  df = df[filters]

  # Alias and re-order columns.
  df['Time'] = df.timestamp
  df['Executed In'] = px.DurationNanos(df.execution_time_ns)
  df['# Bytes'] = px.Bytes(df.bytes_processed)
  df['# Records'] = df.records_processed
  df['Error'] = df.error_message
  df = df['script_id', 'Time', 'Executed In', '# Bytes', '# Records', 'Error']
  ${scriptId ? "df = df.drop('script_id')" : ''}

  return df
`;

  const vis = {
    variables: [{
      name: 'script_id',
      type: 'PX_STRING',
      defaultValue: '""',
    }],
    globalFuncs: [{
      outputName: 'cron_stats',
      func: {
        name: 'cron_stats',
        args: [{ name: 'script_id', variable: 'script_id' }],
      },
    }],
    widgets: [{
      name: 'Export History',
      globalFuncOutputName: 'cron_stats',
      displaySpec: {
        [DISPLAY_TYPE_KEY]: TABLE_DISPLAY_TYPE,
      },
    }],
  };

  const args = { script_id: scriptId || '' };

  return { pxl, vis, args };
}

function getExportScriptStats(
  api: PixieAPIClientAbstract,
  clusterId: string,
  exportScriptId?: string,
): CancellablePromise<VizierTable | { error: VizierQueryError }> {
  if (!api || !clusterId.length) throw new Error('Do not call getExportScriptStats before API and cluster ID are set!');

  let cancelExecution: () => void;
  // Normally, we'd use a promise rejection for the error state (cleaner, easier to read, makes more sense).
  // See below for why that isn't done here.
  const promise = makeCancellable<VizierTable | { error: VizierQueryError }>(new Promise((resolve) => {
    const clusterConfig = {
      id: clusterId,
      passthroughClusterAddress: window.location.origin +  (PASSTHROUGH_PROXY_PORT ? `:${PASSTHROUGH_PROXY_PORT}` : ''),
    };

    const { pxl, vis, args } = getStatsScript(exportScriptId);
    const runner = api.executeScript(
      clusterConfig,
      pxl,
      { enableE2EEncryption: true },
      getQueryFuncs(vis, args, 'Export History'),
      'CRON_STATS',
    );

    const subscription = runner.subscribe((update: ExecutionStateUpdate) => {
      switch (update.event.type) {
        case 'start':
          cancelExecution = () => {
            update.cancel();
            subscription.unsubscribe();
            cancelExecution = null;
          };
          break;
        case 'stats': {
          const { tables } = update.results;
          resolve(tables[0]);
          break;
        }
        case 'data':
        case 'metadata':
        case 'mutation-info':
        case 'status':
          break;
        case 'cancel':
          promise.cancel();
          break;
        case 'error':
          // Should be a reject, but if that rejection is caught, browsers still emits an Unhandled Rejection error.
          // Why it happens for this promise is not clear at time of writing and wasn't worth troubleshooting further.
          resolve({ error: update.event.error });
          break;
        default:
          checkExhaustive(update.event);
      }
    });
  }), false, () => cancelExecution?.());

  return promise;
}

/**
 * Fetch cron stats for data export scripts on a given cluster. If a specific export script's ID is provided, filters.
 * @param clusterId Which cluster to check - this will run a PxL script on it.
 * @param exportScriptId If provided, results will be filtered only to those applicable to it.
 * @returns Loading and error state; an array of found records.
 */
export function useExportScriptStats(clusterId: string, exportScriptId?: string): {
  loading: boolean;
  error?: VizierQueryError;
  table?: VizierTable;
} {
  const [loading, setLoading] = React.useState(true);
  const [error, setError] = React.useState(null);
  const [table, setTable] = React.useState<VizierTable>(null);

  const api = React.useContext(PixieAPIContext);

  React.useEffect(() => {
    if (!api || !clusterId) return;
    setLoading(true);
    setError(null);
    setTable(null);
    const promise = getExportScriptStats(api, clusterId, exportScriptId);
    promise
      .then((result) => {
        const isError = !!(result as any)?.error;
        if (isError) {
          setError((result as { error: VizierQueryError }).error);
        } else {
          setTable(result as VizierTable);
        }
        setLoading(false);
      });

    return () => promise.cancel();
  }, [api, clusterId, exportScriptId]);

  return React.useMemo(() => ({ loading, error, table }), [loading, error, table]);
}

type ScriptId = string;

function aggregateStats(all: Array<VizierTable | { error: VizierQueryError }>): {
  unavailable: number;
  map: Map<ScriptId, { pass: number, fail: number }>;
} {
  let unavailable = 0;
  const map = new Map<ScriptId, { pass: number, fail: number }>();

  for (const result of all) {
    const isError = !!(result as any)?.error;
    if (isError) {
      unavailable++;
    } else {
      const { rows } = result as VizierTable;
      for (const { script_id: scriptId, Error: error } of rows) {
        const state = map.get(scriptId) ?? { pass: 0, fail: 0 };
        if (error) state.fail++;
        else state.pass++;
        map.set(scriptId, state);
      }
    }
  }

  return { unavailable, map };
}
interface ContextProps {
  loading: boolean;
  unavailableClusters: number;
  status: Map<ScriptId, { pass: number, fail: number }>;
}

export const ExportStatusContext = React.createContext<ContextProps>({
  loading: true,
  unavailableClusters: 0,
  status: new Map(),
});

/**
 * Given a list of retention scripts, queries their execution history on all clusters they're configured to run on.
 * Collects those results by script ID for presentation.
 */
export const ExportStatusContextProvider = React.memo<WithChildren<{ scripts: GQLRetentionScript[] }>>(({
  scripts,
  children,
}) => {
  const { clusters } = useClustersForRetentionScripts();

  const selectedClusterIds = React.useMemo(() => {
    const out = new Set<string>();
    for (const script of scripts) {
      for (const id of script.clusters) {
        out.add(id);
      }
    }
    return out;
  }, [scripts]);

  // A retention script that has no clusters explicitly selected runs on all of them by default. Detect that.
  const queryAllConnectedClusters = React.useMemo(() => (
    scripts.some(script => script.enabled && !script.clusters.length)
  ), [scripts]);

  const clustersToQuery = React.useMemo(() => {
    return clusters?.filter(
      c => (queryAllConnectedClusters && c.status !== GQLClusterStatus.CS_DISCONNECTED) || selectedClusterIds.has(c.id),
    ) ?? [];
  }, [clusters, queryAllConnectedClusters, selectedClusterIds]);

  // To prevent bounciness
  const stableClusterIds = React.useMemo(() => (
    clustersToQuery.map(c => c.id).sort().join(';')
  ), [clustersToQuery]);

  const [loading, setLoading] = React.useState(true);
  const [unavailableClusters, setUnavailableClusters] = React.useState(0);
  const [status, setStatus] = React.useState(new Map());

  const api = React.useContext(PixieAPIContext);

  React.useEffect(() => {
    if (!api || !clustersToQuery.length) return;

    setLoading(true);
    setUnavailableClusters(0);
    setStatus(new Map());
    const promises = clustersToQuery.map(({ id }) => getExportScriptStats(api, id));
    Promise.all(promises).then((allResults) => {
      setLoading(false);
      const { unavailable, map } = aggregateStats(allResults);
      setUnavailableClusters(unavailable);
      setStatus(map);
    }).catch((err) => {
      console.error('Something went wrong when fetching stats from all clusters...', err);
      setLoading(false);
      setUnavailableClusters(0);
      setStatus(new Map());
    });

    return () => {
      for (const p of promises) {
        p.cancel();
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [stableClusterIds]);

  const ctx = React.useMemo(() => ({
    loading,
    unavailableClusters,
    status,
  }), [loading, unavailableClusters, status]);
  return <ExportStatusContext.Provider value={ctx}>{children}</ExportStatusContext.Provider>;
});
ExportStatusContextProvider.displayName = 'ExportStatusContextProvider';
