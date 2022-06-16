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

import { ExecutionStateUpdate, PixieAPIContext, VizierQueryError, VizierTable } from 'app/api';
import { PASSTHROUGH_PROXY_PORT } from 'app/containers/constants';
import { DISPLAY_TYPE_KEY, getQueryFuncs, TABLE_DISPLAY_TYPE, Vis } from 'app/containers/live/vis';
import { checkExhaustive } from 'app/utils/check-exhaustive';

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
  const [result, setResult] = React.useState<VizierTable>(null);

  const api = React.useContext(PixieAPIContext);

  React.useEffect(() => {
    setLoading(true);
    setError(null);
    setResult(null);

    if (!clusterId?.length) return;

    let cancelExecution: () => void;

    const clusterConfig = {
      id: clusterId,
      attachCredentials: true,
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
            cancelExecution = null;
          };
          break;
        case 'stats': {
          const { tables } = update.results;
          setResult(tables[0]);
          setLoading(false);
          break;
        }
        case 'data':
        case 'metadata':
        case 'mutation-info':
        case 'status':
          break;
        case 'cancel':
          setLoading(false);
          setResult(null);
          break;
        case 'error':
          setLoading(false);
          setResult(null);
          setError(update.event.error);
          break;
        default:
          checkExhaustive(update.event);
      }
    });

    return () => {
      subscription.unsubscribe();
      cancelExecution?.();
    };
  }, [api, clusterId, exportScriptId]);

  return React.useMemo(() => ({
    error,
    loading,
    table: result,
  }), [error, loading, result]);
}
