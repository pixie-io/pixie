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
  useHistory,
} from 'react-router-dom';
import { Observable } from 'rxjs';

import {
  PixieAPIContext, ExecutionStateUpdate, VizierQueryError, GRPCStatusCode,
} from 'app/api';
import { ClusterContext, useClusterConfig } from 'app/common/cluster-context';
import { useSnackbar } from 'app/components';
import { LiveRouteContext, push } from 'app/containers/App/live-routing';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { getQueryFuncs } from 'app/containers/live/vis';
import { ResultsContext } from 'app/context/results-context';
import pixieAnalytics from 'app/utils/analytics';
import {
  argsForVis, Arguments, stableSerializeArgs, validateArgs,
} from 'app/utils/args-utils';
import { checkExhaustive } from 'app/utils/check-exhaustive';
import { containsMutation, isStreaming } from 'app/utils/pxl';
import { WithChildren } from 'app/utils/react-boilerplate';
import { Script } from 'app/utils/script-bundle';

const NUM_MUTATION_RETRIES = 5;
const MUTATION_RETRY_MS = 5000; // 5s.

export interface ScriptContextProps {
  /**
   * The currently selected script, including any local edits the user has made.
   */
  script: Script;
  /** Args that will be passed to the current script if it's executed. Mirrored from LiveRouteContext. */
  args: Record<string, string | string[]>;
  /**
   * Updates the script and args that will be used if execute() is called.
   */
  setScriptAndArgs: (script: Script, args: Record<string, string | string[]>) => void;
  /**
   * Updates the script and args that will be used if execute() is called by a user manually running execute
   * through the hot-key or button.
   */
  setScriptAndArgsManually: (script: Script, args: Record<string, string | string[]>) => void;
  /** Runs the currently selected scripts, with the current args and any user-made edits to the PXL/Vis/etc. */
  execute: () => void;
  /**
   * If there is a script currently running, cancels that execution.
   * This happens automatically when running a new script; it should only need to be called manually for things like
   * navigating away from the live view entirely or for certain error scenarios.
   */
  cancelExecution: () => void;
  manual: boolean;

  /** Generates an export script from the current script and args. */
  generateOTelExportScript: (script: string) => Promise<void>;
}

export const ScriptContext = React.createContext<ScriptContextProps>({
  script: null,
  args: {},
  manual: false,
  setScriptAndArgs: () => {},
  setScriptAndArgsManually: () => {},
  execute: () => {},
  cancelExecution: () => {},
  generateOTelExportScript: async () => {},
});
ScriptContext.displayName = 'ScriptContext';

export const ScriptContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const apiClient = React.useContext(PixieAPIContext);
  const {
    scriptId,
    args,
    embedState,
  } = React.useContext(LiveRouteContext);
  const { selectedClusterName, loading: loadingCluster } = React.useContext(ClusterContext);
  const { scripts: availableScripts, loading: loadingAvailableScripts } = React.useContext(ScriptsContext);
  const resultsContext = React.useContext(ResultsContext);
  const showSnackbar = useSnackbar();

  const clusterConfig = useClusterConfig();

  const [script, setScript] = React.useState<Script>(null);
  const [manual, setManual] = React.useState(false);

  // When the user changes the script entirely (like via breadcrumbs or a fresh navigation): reset PXL, vis, etc.
  React.useEffect(() => {
    if (!loadingAvailableScripts && availableScripts.has(scriptId)) {
      const scriptObj = availableScripts.get(scriptId);
      if (!scriptObj) {
        return;
      }
      if (scriptObj.id === SCRATCH_SCRIPT.id) {
        setScript((prevScript) => {
          if (prevScript) {
            return prevScript;
          }
          return scriptObj;
        });
      } else {
        setScript(scriptObj);
      }
    }
  }, [scriptId, loadingAvailableScripts, availableScripts]);

  const serializedArgs = stableSerializeArgs(args);

  // Per-execution minutia
  const [runningExecution, setRunningExecution] = React.useState<Observable<ExecutionStateUpdate> | null>(null);
  const [cancelExecution, setCancelExecution] = React.useState<() => void | null>(null);
  const [numExecutionTries, setNumExecutionTries] = React.useState(0);
  const [hasMutation, setHasMutation] = React.useState(false);
  const [probeDeployed, setProbeDeployed] = React.useState(false);
  const history = useHistory();

  // Timing: execute can be called before the API has finished returning all needed data, because VizierRoutingContext
  // does not depend on the API and can update (triggering ScriptLoader) before required data has loaded for execution.
  const readyToExecute = !loadingAvailableScripts && !loadingCluster;
  const [awaitingExecution, setAwaitingExecution] = React.useState(false);

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const execute: () => void = React.useCallback(() => {
    if (!readyToExecute) {
      setAwaitingExecution(true);
      return;
    }

    if (!apiClient) throw new Error('Tried to execute a script before PixieAPIClient was ready!');
    if (!script || !clusterConfig || !args) {
      throw new Error('Tried to execute before script, cluster connection, and/or args were ready!');
    }

    const validationError = validateArgs(script.vis, args);
    if (validationError != null) {
      resultsContext.setResults({
        error: validationError,
        tables: new Map(),
      });
      return;
    }

    cancelExecution?.();

    if (containsMutation(script.code) && manual) {
      setNumExecutionTries(NUM_MUTATION_RETRIES);
    } else if (containsMutation(script.code) && !manual) {
      // We should call execute() even when the mutation wasn't manually executed.
      // This will trigger the proper loading states so that if someone directly
      // opened the page to a mutation script, their cluster loading state resolves properly.
      setNumExecutionTries(0);
    } else {
      setNumExecutionTries(1);
    }

    const execution = apiClient.executeScript(
      clusterConfig,
      script.code,
      { enableE2EEncryption: true },
      getQueryFuncs(script.vis, args, embedState.widget),
      script.id,
    );
    setRunningExecution(execution);
    setManual(false);
    resultsContext.clearResults();
    resultsContext.setLoading(true);
    resultsContext.setStreaming(isStreaming(script.code));
    setHasMutation(containsMutation(script.code));
    setProbeDeployed(false);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [apiClient, script, embedState.widget, clusterConfig, serializedArgs, cancelExecution, manual]);

  // As above: delay first execution if required information isn't ready yet.
  React.useEffect(() => {
    if (awaitingExecution && readyToExecute) {
      execute();
      setAwaitingExecution(false);
    }
  }, [readyToExecute, awaitingExecution, execute]);

  // Retry timer, which only applies to mutation scripts.
  // If a mutation script is running for more than MUTATION_RETRY_MS and still
  // waiting for probes to deploy, the execution will stop and retry.
  React.useEffect(() => {
    if (!hasMutation || probeDeployed || !runningExecution || numExecutionTries <= 0) {
      return () => {};
    }

    const timeout = setTimeout(() => {
      setNumExecutionTries(numExecutionTries - 1);
    }, MUTATION_RETRY_MS);

    // If the script restarts for any reason or its probe finishes deploying, clear the timer.
    return () => {
      clearTimeout(timeout);
    };
  }, [hasMutation, probeDeployed, runningExecution, numExecutionTries]);

  React.useEffect(() => {
    if (numExecutionTries <= 0) {
      resultsContext.setLoading(false);
      return () => {};
    }

    let cleanup = () => {};

    const subscription = runningExecution?.subscribe((update: ExecutionStateUpdate) => {
      switch (update.event.type) {
        case 'start':
          // Cleanup is called when the React hook is cleaned up. This contains a subset
          // of the functions called when an execution is cancelled. This is to handle
          // retries for mutations, since the script loading/mutation/streaming state
          // should not be completely reset.
          cleanup = () => {
            update.cancel();
            setCancelExecution(null);
          };
          setCancelExecution(() => () => {
            update.cancel();
            setHasMutation(false);
            resultsContext.setStreaming(false);
            resultsContext.setLoading(false);
            setNumExecutionTries(0);
            setCancelExecution(null);
          });
          break;
        case 'data': {
          // Force an update that React will pick up on. New batches have already been appended to the tables here.
          resultsContext.setResults((prev) => ({ ...prev }));
          if (resultsContext.streaming) {
            resultsContext.setLoading(false);
          }
          break;
        }
        case 'metadata':
        case 'mutation-info':
        case 'status':
        case 'stats':
          // Mutation schema not ready yet.
          if (hasMutation && update.results.mutationInfo) {
            const mutationStatusCode = update.results.mutationInfo.getStatus().getCode();
            if (mutationStatusCode === GRPCStatusCode.Unavailable) {
              resultsContext.setResults({ tables: new Map(), mutationInfo: update.results.mutationInfo });
              setProbeDeployed(false);
            } else if (mutationStatusCode === GRPCStatusCode.OK) {
              resultsContext.setResults((prev) => ({
                ...prev,
                mutationInfo: update.results.mutationInfo,
              }));
              setProbeDeployed(true);
            }
          }

          if (update.results && (resultsContext.streaming || update.results.executionStats)) {
            resultsContext.setResults({
              error: resultsContext.error,
              stats: update.results.executionStats,
              mutationInfo: resultsContext.mutationInfo,
              tables: update.results.tables.reduce(
                (map, table) => map.set(table.name, table),
                new Map()),
            });
          }
          // Query completed normally
          if (update.results.executionStats) {
            setCancelExecution(null);
            resultsContext.setLoading(false);
            resultsContext.setStreaming(false);
            setNumExecutionTries(0);
            setHasMutation(false);
            pixieAnalytics.track('Query Execution', {
              status: 'success',
              query: script.code,
              queryId: update.results.queryId,
              title: script.id,
            });
          }
          break;
        case 'error': {
          const error = Array.isArray(update.event.error) ? update.event.error[0] : update.event.error;
          const { errType } = (error as VizierQueryError);

          if (hasMutation && errType === 'unavailable') {
            // Ignore unavailable errors from the mutation executor.
            break;
          }

          const errMsg = error.message;
          resultsContext.setResults({ error, tables: new Map() });
          resultsContext.setLoading(false);
          resultsContext.setStreaming(false);
          setNumExecutionTries(numExecutionTries - 1);

          pixieAnalytics.track('Query Execution', {
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
      }
    });
    return () => {
      cleanup();
      subscription?.unsubscribe();
    };
    // ONLY watch runningExecution for this. This effect only subscribes/unsubscribes from it.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [runningExecution, numExecutionTries]);

  const setScriptAndArgs = React.useCallback((newScript: Script, newArgs: Arguments = args) => {
    setScript(newScript);
    setManual(false);

    push(selectedClusterName, newScript.id, argsForVis(newScript.vis, newArgs), embedState);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [serializedArgs, embedState, push, selectedClusterName]);

  const setScriptAndArgsManually = React.useCallback((newScript: Script, newArgs: Arguments = args) => {
    setScript(newScript);
    setManual(true);

    push(selectedClusterName, newScript.id, argsForVis(newScript.vis, newArgs), embedState);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [serializedArgs, embedState, push, selectedClusterName]);

  const generateOTelExportScript: (pxl: string) => Promise<void> = React.useCallback((pxl: string) => {
    if (!apiClient) throw new Error('Tried to execute a script before PixieAPIClient was ready!');
    if (!clusterConfig) {
      throw new Error('Tried to execute before script, cluster connection, and/or args were ready!');
    }

    return apiClient.generateOTelExportScript(
      clusterConfig,
      pxl,
    ).then((res: string | VizierQueryError) => {
      if (typeof res !== 'string') {
        const error = res as VizierQueryError;
        if (error?.details === 'context deadline exceeded') {
          error.details = 'Context deadline exceeded. ' +
            'Generate OTel script is not supported on this cluster, please upgrade your installation';
        }
        // Then it's an error.
        resultsContext.clearResults();
        resultsContext.setResults({ error, tables: new Map() });
        return;
      }
      // Navigate to the Plugin creation page.
      history.push('/configure-data-export/create', { contents: res as string });
    });
  }, [apiClient, clusterConfig, resultsContext, history]);

  const context: ScriptContextProps = React.useMemo(() => ({
    script,
    args,
    manual,
    setScriptAndArgs,
    setScriptAndArgsManually,
    execute,
    cancelExecution: (cancelExecution ?? (() => {})),
    generateOTelExportScript,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [script, execute, serializedArgs, selectedClusterName, setScriptAndArgs, setScriptAndArgsManually]);

  return <ScriptContext.Provider value={context}>{children}</ScriptContext.Provider>;
});
ScriptContextProvider.displayName = 'ScriptContextProvider';
