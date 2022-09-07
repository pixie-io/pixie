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

import { VizierQueryError } from 'app/api';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { parseVis, Vis } from 'app/containers/live/vis';
import { ResultsContext } from 'app/context/results-context';
import { argsForVis } from 'app/utils/args-utils';
import { WithChildren } from 'app/utils/react-boilerplate';

import { SetStateFunc } from './common';
import { ScriptContext } from './script-context';

export interface EditorContextProps {
  setVisEditorText: SetStateFunc<string>;
  setPxlEditorText: SetStateFunc<string>;
  pxlEditorText: string;
  saveEditor: () => void;
}

export const EditorContext = React.createContext<EditorContextProps>(null);
EditorContext.displayName = 'EditorContext';

export const EditorContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const resultsContext = React.useContext(ResultsContext);

  const {
    script, setScriptAndArgsManually, args,
  } = React.useContext(ScriptContext);

  const {
    setScratchScript,
  } = React.useContext(ScriptsContext);

  const [visEditorText, setVisEditorText] = React.useState<string>('');
  const [pxlEditorText, setPxlEditorText] = React.useState<string>('');

  // Saves the editor values in the script.
  const saveEditor = React.useCallback(() => {
    // Don't keep setting the script when spamming the "execute" hotkey. Once is enough.
    if (resultsContext.loading) return;

    // Parse Vis for any possible formatting errors.
    let vis: Vis;
    try {
      vis = parseVis(visEditorText);
    } catch (e) {
      resultsContext.setResults({
        error: new VizierQueryError('vis', ['While parsing Vis Spec: ', e.toString()]),
        tables: new Map(),
      });
      return;
    }

    const pxlDirty = pxlEditorText !== script.code;
    const visDirty = JSON.stringify(vis) !== JSON.stringify(script.vis);

    const id = (pxlDirty || visDirty) ? SCRATCH_SCRIPT.id : script.id;
    const newArgs = visDirty ? argsForVis(vis, args) : args;

    const scratchScript = {
      ...script, id, code: pxlEditorText, vis,
    };
    if (pxlDirty || visDirty) {
      setScratchScript(scratchScript);
    }
    setScriptAndArgsManually(scratchScript, newArgs);
  }, [
    setScratchScript, setScriptAndArgsManually, script, args,
    pxlEditorText, visEditorText, resultsContext,
  ]);

  // Update the text when the script changes, but not edited.
  React.useEffect(() => {
    if (!script) {
      return;
    }
    setPxlEditorText(script.code);
    setVisEditorText(JSON.stringify(script.vis, undefined, 4));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [script?.code, script?.vis]);

  const value = React.useMemo(() => ({
    setPxlEditorText, setVisEditorText, saveEditor, pxlEditorText,
  }), [setPxlEditorText, setVisEditorText, saveEditor, pxlEditorText]);

  return (
    <EditorContext.Provider value={value}>
      {children}
    </EditorContext.Provider>
  );
});
EditorContextProvider.displayName = 'EditorContextProvider';
