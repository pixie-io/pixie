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
import { VizierRouteContext } from 'containers/App/vizier-routing';
import { ScriptsContext } from 'containers/App/scripts-context';
import { getQueryFuncs, parseVis, Vis } from 'containers/live/vis';
import { Script } from 'utils/script-bundle';
import { PixieAPIContext } from '@pixie-labs/api-react';
import { ExecutionStateUpdate } from '@pixie-labs/api';
import { Observable } from 'rxjs';
import { ClusterContext } from 'common/cluster-context';
import { checkExhaustive } from 'utils/check-exhaustive';

export interface ParsedScript extends Omit<Script, 'vis'> {
  vis: Vis;
}

export interface ScriptContextProps {
  /** The currently selected script, including any local edits the user has made, with the Vis spec parsed. */
  script: ParsedScript;
  /** Args that will be passed to the current script if it's executed. Mirrored from VizierRouteContext. */
  args: Record<string, string|string[]>;
  /** The original script corresponding to the active script's ID (from ScriptContext) */
  baseScript: Script;
  /**
   * If manuallyEdited is true, the user has changed the script's code, vis spec, or other metadata in the editor.
   * baseScript will remain unchanged when this happens, to quickly check what the unedited version looks like.
   */
  manuallyEdited: boolean;
  /** Changes the code to be run. Sets manuallyEdited to true as a side effect. */
  setPxl: (pxl: string) => void;
  /** Changes the vis spec of the script to be run. Sets manuallyEdited to true as a side effect. */
  setVis: (vis: string) => void;
}

export const ScriptContext = React.createContext<ScriptContextProps>({
  script: null,
  args: {},
  baseScript: null,
  manuallyEdited: false,
  setPxl: () => {},
  setVis: () => {},
});

export const ScriptContextProvider: React.FC = ({ children }) => {
  const apiClient = React.useContext(PixieAPIContext);
  const { script: scriptId, args } = React.useContext(VizierRouteContext);
  const { scripts: availableScripts, loading: loadingAvailableScripts } = React.useContext(ScriptsContext);
  const { selectedCluster } = React.useContext(ClusterContext);

  const [uneditedScript, setUneditedScript] = React.useState<Script>(null);
  const [script, setScript] = React.useState<ParsedScript>(null);

  const [manuallyEdited, setManuallyEdited] = React.useState(false);

  // When the user changes the script entirely (like via breadcrumbs or a fresh navigation): reset PXL, vis, etc.
  React.useEffect(() => {
    if (!loadingAvailableScripts && availableScripts.has(scriptId)) {
      const scriptObj = availableScripts.get(scriptId);
      if (scriptObj.id && scriptObj.vis && scriptObj.code) {
        setUneditedScript(scriptObj);
        setScript({ ...scriptObj, vis: parseVis(scriptObj.vis || '{}') });
        setManuallyEdited(false);
      }
    }
  }, [scriptId, loadingAvailableScripts, availableScripts]);

  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());

  const [runningExecution, setRunningExecution] = React.useState<Observable<ExecutionStateUpdate>|null>(null);
  const [cancelExecution, setCancelExecution] = React.useState<() => void>(null);

  // TODO(nick,PC-917): Export this in the context once it works and updates ResultsContext appropriately
  //  Must retain the validation logic for the vis spec (should that live outside of this file too? In vis.tsx even?)
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const execute: () => void = React.useMemo(() => () => {
    if (!apiClient) throw new Error('Tried to execute a script before PixieAPIClient was ready!');
    if (!script?.vis || !selectedCluster || !args) throw new Error('Must specify a script and args to execute!');

    cancelExecution?.();

    const execution = apiClient.executeScript(
      selectedCluster,
      scriptId,
      // TODO(nick): Once cluster is separated from args in VizierRoutingContext, just pass args as-is here.
      getQueryFuncs(script.vis, { ...args, cluster: undefined }),
    );
    setRunningExecution(execution);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [apiClient]);

  /*
   * TODO(nick): Copy over most of the basic behaviors from the old ScriptContext here. Simplify where feasible.
   *  In particular, merge the mutations and non-mutation paths if possible (difference is mostly about retries).
   *  Set data in ResultsContext (new ResultsContextProvider has to wrap new ScriptContextProvider to do that).
   *  Test if unsubscribing in cleanup like below is sufficient to dodge the timeout issue from the last version.
   *  If it isn't, might need to be able to cancel the call instead in cleanup. What implications does that have?
   */
  React.useEffect(() => {
    const subscription = runningExecution?.subscribe((update: ExecutionStateUpdate) => {
      switch (update.event.type) {
        case 'start':
          setCancelExecution(() => () => {
            update.cancel();
            setCancelExecution(undefined);
          });
          break;
        case 'data':
          // TODO(nick): onUpdate(update.event.data); Consider mutation
          break;
        case 'metadata':
        case 'mutation-info':
        case 'status':
        case 'stats':
          // TODO(nick): onResults(update.results); Consider mutation; consider closing the stream if applicable
          break;
        case 'error':
          // TODO(nick): onError(update.event.error); Consider mutation
          break;
        case 'cancel':
          break;
        default:
          checkExhaustive(update.event);
          break;
      }
    });
    return () => {
      subscription?.unsubscribe();
    };
  }, [runningExecution]);

  const context: ScriptContextProps = React.useMemo(() => ({
    script,
    args,
    baseScript: uneditedScript,
    manuallyEdited,
    setPxl: (pxl: string) => {
      setScript({ ...script, code: pxl });
      setManuallyEdited(true);
    },
    setVis: (vis: string) => {
      setScript({ ...script, vis: parseVis(vis) });
      setManuallyEdited(true);
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [script, uneditedScript, manuallyEdited, serializedArgs]);

  return <ScriptContext.Provider value={context}>{children}</ScriptContext.Provider>;
};
