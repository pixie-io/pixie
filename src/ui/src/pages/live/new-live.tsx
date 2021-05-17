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
import { VizierContextRouter } from 'containers/App/vizier-routing';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import { ScriptContext, ScriptContextProvider } from 'context/new-script-context';
import LiveViewBreadcrumbs from 'containers/live/new-breadcrumbs';
import { ScriptLoader } from 'containers/live/new-script-loader';
import { Button } from '@material-ui/core';
import { Link } from 'react-router-dom';
import { ClusterContext } from 'common/cluster-context';
import { ResultsContext, ResultsContextProvider } from 'context/results-context';

import { EditorSplitPanel } from 'containers/editor/new-editor';
import { scrollbarStyles } from '@pixie-labs/components';
import {
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';
import { LayoutContext, LayoutContextProvider } from 'context/layout-context';
import EditorContextProvider, { EditorContext } from 'context/editor-context';
import LiveViewShortcutsProvider from 'containers/live/shortcuts';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    backgroundColor: theme.palette.background.default,
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  content: {
    marginLeft: theme.spacing(6),
    marginTop: theme.spacing(2),
    display: 'flex',
    flex: 1,
    minWidth: 0,
    minHeight: 0,
    flexDirection: 'column',
    [theme.breakpoints.down('sm')]: {
      // Sidebar is disabled.
      marginLeft: 0,
    },
  },
}));

const LiveView: React.FC = () => {
  const classes = useStyles();
  const { selectedClusterName } = React.useContext(ClusterContext);
  const { script, args, routeFor } = React.useContext(ScriptContext);
  const results = React.useContext(ResultsContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { setEditorPanelOpen } = React.useContext(LayoutContext);

  const visSlice = React.useMemo(() => {
    if (!script) {
      return '';
    }
    return JSON.stringify(script, null, 2);
  }, [script]);

  const hotkeyHandlers = {
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    execute: () => saveEditor(),
    // TODO(philkuz,PC-917) enable the other commands when their components are added in.
    'toggle-data-drawer': () => {},
    'pixie-command': () => {},
  };

  if (!selectedClusterName || !script || !args) return null;

  const nextRoute = (script.id === 'px/cluster')
    ? routeFor('px/pod', { cluster: selectedClusterName, namespace: 'barNamespace', pod: 'bazPod' })
    : routeFor('px/cluster', { cluster: selectedClusterName }); // Ensure that the defaults work

  return (
    <div className={classes.root}>
      <div className={classes.content}>
        <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
          <EditorSplitPanel>
            <div style={{ display: 'flex', flexFlow: 'column nowrap' }}>
              <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>
                {script.id}
                (
                {JSON.stringify(args)}
                )
              </pre>
              <LiveViewBreadcrumbs />
              <Button
                component={Link}
                variant='contained'
                color='primary'
                to={nextRoute}
              >
                Try another route
              </Button>
              <Button
                variant='contained'
                color='secondary'
                onClick={saveEditor}
              >
                Save Pxl
              </Button>
              <h2>Script body</h2>
              <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>{visSlice}</pre>
              {results.tables && (
                <>
                  <h2>Results of latest script run</h2>
                  <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>
                    {JSON.stringify(results, null, 2)}
                  </pre>
                </>
              )}
            </div>
          </EditorSplitPanel>
        </LiveViewShortcutsProvider>
      </div>
    </div>
  );
};

// TODO(nick,PC-917): withLiveViewContext needs a new version too that can do this. Complexity: VizierContextRouter
//  isn't just a context, it does manipulate behavior. Should it be in there? It's midway inside the stack; hard to move
const ContextualizedLiveView: React.FC = () => (
  <LayoutContextProvider>
    <ScriptsContextProvider>
      <VizierContextRouter>
        <ResultsContextProvider>
          <ScriptContextProvider>
            <EditorContextProvider>
              <ScriptLoader />
              <LiveView />
            </EditorContextProvider>
          </ScriptContextProvider>
        </ResultsContextProvider>
      </VizierContextRouter>
    </ScriptsContextProvider>
  </LayoutContextProvider>
);

export default ContextualizedLiveView;
