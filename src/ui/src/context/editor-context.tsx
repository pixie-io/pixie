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

import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { VizierQueryError } from 'app/api';
import { ResultsContext } from 'app/context/results-context';
import { parseVis, Vis } from 'app/containers/live/vis';
import { SetStateFunc } from './common';

import { ScriptContext } from './script-context';
import { LayoutContext } from './layout-context';

export interface EditorContextProps {
  setVisEditorText: SetStateFunc<string>;
  setPxlEditorText: SetStateFunc<string>;
  saveEditor: () => void;
}

export const EditorContext = React.createContext<EditorContextProps>(null);

const EditorContextProvider: React.FC = ({ children }) => {
  const resultsContext = React.useContext(ResultsContext);

  const {
    script, setScriptAndArgsManually, args,
  } = React.useContext(ScriptContext);

  const {
    setScratchScript,
  } = React.useContext(ScriptsContext);

  const {
    editorPanelOpen,
  } = React.useContext(LayoutContext);

  const [visEditorText, setVisEditorText] = React.useState<string>('');
  const [pxlEditorText, setPxlEditorText] = React.useState<string>('');

  // Saves the editor values in the script.
  const saveEditor = React.useCallback(() => {
    const id = editorPanelOpen ? SCRATCH_SCRIPT.id : script.id;

    // Parse Vis for any possible formatting errors.
    let vis: Vis;
    try {
      vis = parseVis(visEditorText);
    } catch (e) {
      resultsContext.setResults({
        error: new VizierQueryError('vis', ['While parsing Vis Spec: ', e.toString()]),
        tables: {},
      });
      return;
    }

    const scratchScript = {
      ...script, id, code: pxlEditorText, vis,
    };
    if (editorPanelOpen) {
      setScratchScript(scratchScript);
    }
    setScriptAndArgsManually(scratchScript, args);
  }, [
    setScratchScript, setScriptAndArgsManually, script, args,
    editorPanelOpen, pxlEditorText, visEditorText, resultsContext,
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

  return (
    <EditorContext.Provider
      value={{
        setPxlEditorText,
        setVisEditorText,
        saveEditor,
      }}
    >
      {children}
    </EditorContext.Provider>
  );
};

export default EditorContextProvider;
