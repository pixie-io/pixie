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
import { VizierRouteContext, VizierRouteContextProps } from 'containers/App/vizier-routing';
import { ScriptsContext } from 'containers/App/scripts-context';
import { getQueryFuncs, parseVis, Vis } from 'containers/live/vis';
import { Script } from 'utils/script-bundle';
import { PixieAPIContext, useListClusters } from '@pixie-labs/api-react';
import {
  containsMutation, ExecutionStateUpdate, isStreaming, VizierQueryError, ClusterConfig,
} from '@pixie-labs/api';
import { Observable } from 'rxjs';
import { checkExhaustive } from 'utils/check-exhaustive';
import { ResultsContext } from 'context/results-context';
import { useSnackbar } from '@pixie-labs/components';
import { validateArgs } from 'utils/new-args-utils';

export interface ParsedScript extends Omit<Script, 'vis'> {
  visString: string;
  vis: Vis;
}

export interface ScriptContextProps {
  /**
   * The currently selected script, including any local edits the user has made, with the Vis spec parsed.
   */
  script: ParsedScript;
  /** Args that will be passed to the current script if it's executed. Mirrored from VizierRouteContext. */
  args: Record<string, string|string[]>;
  /**
   * Updates the script (including manual user edits) and args that will be used if execute() is called.
   */
  setScriptAndArgs: (script: Script|ParsedScript, args: Record<string, string|string[]>) => void;
  /**
   * True if and only if the current args satisfy the current script's vis spec. That means:
   * - All args exist in the vis spec, and have valid values.
   * - All variables in the vis spec either have a default value, or are provided in args.
   */
  argsValid: boolean;
  /** Use for clickable <Link>s. Provides a route that, when navigated to, would setScriptAndArgs to match. */
  routeFor: VizierRouteContextProps['routeFor'];
  /** Runs the currently selected scripts, with the current args and any user-made edits to the PXL/Vis/etc. */
  execute: () => void;
  /**
   * If there is a script currently running, cancels that execution.
   * This happens automatically when running a new script; it should only need to be called manually for things like
   * navigating away from the live view entirely or for certain error scenarios.
   */
  cancelExecution: () => void;
}

export const ScriptContext = React.createContext<ScriptContextProps>({
  script: null,
  args: {},
  argsValid: false,
  setScriptAndArgs: () => {},
  routeFor: () => ({}),
  execute: () => {},
  cancelExecution: () => {},
});

export const ScriptContextProvider: React.FC = ({ children }) => {
  const apiClient = React.useContext(PixieAPIContext);
  const {
    scriptId, clusterName, args, push, routeFor,
  } = React.useContext(VizierRouteContext);
  const { scripts: availableScripts, loading: loadingAvailableScripts } = React.useContext(ScriptsContext);
  const resultsContext = React.useContext(ResultsContext);
  const showSnackbar = useSnackbar();

  const [clusters, loadingClusters] = useListClusters();
  const clusterConfig: ClusterConfig|null = React.useMemo(() => {
    if (loadingClusters || !clusters.length) return null;
    const selected = clusters.find((c) => c.clusterName === clusterName);
    if (!selected) return null;

    const passthroughClusterAddress = selected.vizierConfig.passthroughEnabled ? window.location.origin : undefined;
    return selected ? {
      id: selected.id,
      attachCredentials: true,
      passthroughClusterAddress,
    } : null;
  }, [clusters, loadingClusters, clusterName]);

  const [script, setScript] = React.useState<ParsedScript>(null);

  // When the user changes the script entirely (like via breadcrumbs or a fresh navigation): reset PXL, vis, etc.
  React.useEffect(() => {
    if (!loadingAvailableScripts && availableScripts.has(scriptId)) {
      const scriptObj = availableScripts.get(scriptId);
      if (scriptObj.id && scriptObj.vis && scriptObj.code) {
        setScript({ ...scriptObj, visString: scriptObj.vis, vis: parseVis(scriptObj.vis || '{}') });
      }
    }
  }, [scriptId, loadingAvailableScripts, availableScripts]);

  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());
  // TODO(nick,PC-917): Once cluster is separated from args in VizierRoutingContext, use args as-is here.
  const cleanedArgs = { ...args, cluster: undefined, script: undefined };

  // Per-execution minutia
  const [runningExecution, setRunningExecution] = React.useState<Observable<ExecutionStateUpdate>|null>(null);
  const [cancelExecution, setCancelExecution] = React.useState<() => void|null>(null);
  const [hasMutation, setHasMutation] = React.useState(false);

  // Timing: execute can be called before the API has finished returning all needed data, because VizierRoutingContext
  // does not depend on the API and can update (triggering ScriptLoader) before required data has loaded for execution.
  const readyToExecute = !loadingClusters && !loadingAvailableScripts;
  const [awaitingExecution, setAwaitingExecution] = React.useState(false);

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const execute: () => void = React.useMemo(() => () => {
    if (!readyToExecute) {
      setAwaitingExecution(true);
      return;
    }

    if (!apiClient) throw new Error('Tried to execute a script before PixieAPIClient was ready!');
    if (!script?.vis || !clusterConfig || !args) {
      throw new Error('Tried to execute before script, cluster connection, and/or args were ready!');
    }

    // TODO(nick,PC-917): Once cluster is separated from args in VizierRoutingContext, use args as-is here.
    const validationError = validateArgs(script.vis, cleanedArgs);
    if (validationError != null) {
      const details = Array.isArray(validationError.details) ? validationError.details : [validationError.details];
      if (Array.isArray(validationError.details)) {
        showSnackbar({ message: `Could not run script, args were invalid:\n${details.join('\n')}` });
      } else {
        showSnackbar({ message: `Could not run script, args were invalid: ${details}` });
      }
      return;
    }

    // TODO(nick,PC-917): If hasMutation is true, execute needs to be in a retry loop (or just always do that?)
    if (hasMutation) throw new Error('Not yet implemented: running scripts with mutations in new ScriptContext');

    cancelExecution?.();

    const execution = apiClient.executeScript(
      clusterConfig,
      script.code,
      // TODO(nick,PC-917): Once cluster is separated from args in VizierRoutingContext, use args as-is here.
      getQueryFuncs(script.vis, cleanedArgs),
    );
    setRunningExecution(execution);
    resultsContext.clearResults();
    resultsContext.setLoading(true);
    resultsContext.setStreaming(isStreaming(script.code));
    setHasMutation(containsMutation(script.code));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [apiClient, script, clusterConfig, serializedArgs, cancelExecution, scriptId, resultsContext]);

  // As above: delay first execution if required information isn't ready yet.
  React.useEffect(() => {
    if (awaitingExecution && readyToExecute) {
      execute();
      setAwaitingExecution(false);
    }
  }, [readyToExecute, awaitingExecution, execute]);

  React.useEffect(() => {
    const subscription = runningExecution?.subscribe((update: ExecutionStateUpdate) => {
      switch (update.event.type) {
        case 'start':
          setCancelExecution(() => () => {
            update.cancel();
            setHasMutation(false);
            resultsContext.setStreaming(false);
            resultsContext.setLoading(false);
            setCancelExecution(null);
          });
          break;
        case 'data':
          // TODO(nick): update.event.data has a bunch of batch updates that include partial table updates.
          //  Use that, like we do in the old ScriptContext, to update a memoized list instead of replacing it on every
          //  update. This is a performance improvement (also check if that's still true with the simplified run logic).
          resultsContext.setResults({
            error: resultsContext.error,
            stats: resultsContext.stats,
            mutationInfo: resultsContext.mutationInfo,
            tables: update.results.tables.reduce((a, c) => ({ ...a, [c.name]: c }), {}),
          });
          break;
        case 'metadata':
        case 'mutation-info':
        case 'status':
        case 'stats':
          // TODO(nick): Same performance improvement for tables (though this event happens once, maybe best to refresh)
          if (update.results && (resultsContext.streaming || update.results.executionStats)) {
            resultsContext.setResults({
              error: resultsContext.error,
              stats: update.results.executionStats,
              mutationInfo: resultsContext.mutationInfo,
              tables: update.results.tables.reduce((a, c) => ({ ...a, [c.name]: c }), {}),
            });
          }
          // Query completed normally
          if (update.results.executionStats) {
            // TODO(nick): Make sure that `script` cannot be stale here, and always matches the running execution.
            //  It should, considering the useEffect unsubscription, but double check.
            setCancelExecution(null);
            resultsContext.setLoading(false);
            resultsContext.setStreaming(false);
            setHasMutation(false);
            analytics.track('Query Execution', {
              status: 'success',
              query: script.code,
              queryId: update.results.queryId,
              title: script.id,
            });
          }
          break;
        case 'error': {
          const error = Array.isArray(update.error) ? update.error[0] : update.error;
          resultsContext.setResults({ error, tables: {} });
          const { errType } = (error as VizierQueryError);
          const errMsg = error.message;
          resultsContext.setLoading(false);
          resultsContext.setStreaming(false);

          analytics.track('Query Execution', {
            status: 'failed',
            query: script.code,
            queryID: update.results.queryId,
            error: errMsg,
            title: script.id,
          });

          if (errType === 'server' || !errType) {
            showSnackbar({
              message: errMsg,
              action: () => execute(),
              actionTitle: 'Retry',
              autoHideDuration: 5000,
            });
          }
          break;
        }
        case 'cancel':
          break;
        default:
          checkExhaustive(update.event);
          break;
      }
    });
    return () => {
      cancelExecution?.();
      subscription?.unsubscribe();
    };
    // ONLY watch runningExecution for this. This effect only subscribes/unsubscribes from it.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [runningExecution]);

  const context: ScriptContextProps = React.useMemo(() => ({
    script,
    args,
    argsValid: !!script && !!args && validateArgs(script.vis, cleanedArgs) == null,
    setScriptAndArgs: (newScript: Script|ParsedScript, newArgs: Record<string, string|string[]> = args) => {
      if (typeof newScript.vis === 'string') {
        setScript({ ...newScript, visString: newScript.vis, vis: parseVis(newScript.vis || '{}') });
      } else {
        setScript(newScript as ParsedScript);
      }
      push(newScript.id, newArgs);
    },
    routeFor: (s, a) => routeFor(s, a),
    execute,
    cancelExecution: (cancelExecution ?? (() => {})),
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [script, execute, serializedArgs]);

  return <ScriptContext.Provider value={context}>{children}</ScriptContext.Provider>;
};
