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

import Canvas from 'containers/live/new-canvas';
import { EditorSplitPanel } from 'containers/editor/new-editor';
import { scrollbarStyles, EditIcon } from '@pixie-labs/components';
import {
  makeStyles, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Tooltip from '@material-ui/core/Tooltip';
import IconButton from '@material-ui/core/IconButton';
import MoveIcon from '@material-ui/icons/OpenWith';
import { LayoutContext, LayoutContextProvider } from 'context/layout-context';
import EditorContextProvider, { EditorContext } from 'context/editor-context';
import LiveViewShortcutsProvider from 'containers/live/shortcuts';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { DataDrawerContextProvider } from 'context/data-drawer-context';
import NavBars from 'containers/App/nav-bars';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
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
  title: {
    ...theme.typography.h3,
    marginLeft: theme.spacing(3),
    marginBottom: theme.spacing(0),
    color: theme.palette.primary.main,
    whiteSpace: 'nowrap',
    overflow: 'hidden',
    textOverflow: 'ellipsis',
  },
  mainPanel: {
    flex: 1,
    minHeight: 0,
  },
  moveWidgetToggle: {
    border: 'none',
    borderRadius: '50%',
    color: theme.palette.action.active,
  },
  editorPanel: {
    display: 'flex',
    flexDirection: 'row',
    minHeight: 0,
  },
  canvas: {
    overflowY: 'auto',
    overflowX: 'hidden',
    marginLeft: theme.spacing(0.5),
    height: '100%',
    width: '100%',
  },
  hidden: {
    display: 'none',
  },
  iconActive: {
    width: theme.spacing(2),
    color: theme.palette.primary.main,
  },
  iconInactive: {
    width: theme.spacing(2),
    color: theme.palette.foreground.grey1,
  },
  iconButton: {
    marginRight: theme.spacing(1),
    padding: theme.spacing(0.5),
  },
  iconPanel: {
    marginTop: 0,
    marginLeft: theme.spacing(3),
    [theme.breakpoints.down('sm')]: {
      display: 'none',
    },
  },
}));

const ScriptOptions = ({
  classes, widgetsMoveable, setWidgetsMoveable,
}) => {
  const {
    editorPanelOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);
  return (
    <>
      {
        !isMobile
        && (
          <div className={classes.iconPanel}>
            <Tooltip title={`${editorPanelOpen ? 'Close' : 'Open'} editor`} className={classes.iconButton}>
              <IconButton className={classes.iconButton} onClick={() => setEditorPanelOpen(!editorPanelOpen)}>
                <EditIcon className={editorPanelOpen ? classes.iconActive : classes.iconInactive} />
              </IconButton>
            </Tooltip>
            <Tooltip title={`${widgetsMoveable ? 'Disable' : 'Enable'} move widgets`} className={classes.iconButton}>
              <IconButton onClick={() => setWidgetsMoveable(!widgetsMoveable)}>
                <MoveIcon className={widgetsMoveable ? classes.iconActive : classes.iconInactive} />
              </IconButton>
            </Tooltip>
          </div>
        )
      }
    </>
  );
};

const LiveView: React.FC = () => {
  const classes = useStyles();
  const { selectedClusterName } = React.useContext(ClusterContext);
  const { script, args, routeFor } = React.useContext(ScriptContext);
  const results = React.useContext(ResultsContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { setEditorPanelOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

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
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    'pixie-command': () => {},
  };

  const canvasRef = React.useRef<HTMLDivElement>(null);

  if (!selectedClusterName || !script || !args) return null;

  const nextRoute = (script.id === 'px/cluster')
    ? routeFor('px/pod', { cluster: selectedClusterName, namespace: 'barNamespace', pod: 'bazPod' })
    : routeFor('px/cluster', { cluster: selectedClusterName }); // Ensure that the defaults work

  return (
    <div className={classes.root}>
      <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
        <NavBars />
        <div className={classes.content}>
          <LiveViewBreadcrumbs />
          <EditorSplitPanel>
            <ScriptOptions
              classes={classes}
              widgetsMoveable={widgetsMoveable}
              setWidgetsMoveable={setWidgetsMoveable}
            />
            <div style={{ display: 'flex', flexFlow: 'column nowrap' }}>
              <Button
                variant='contained'
                color='secondary'
                onClick={saveEditor}
              >
                Save Pxl
              </Button>
            </div>
            {
              results.loading ? (<div> loading </div>) : (

                <DataDrawerSplitPanel className={classes.mainPanel}>
                  <div className={classes.canvas} ref={canvasRef}>
                    <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                  </div>
                </DataDrawerSplitPanel>
              )
            }
          </EditorSplitPanel>
        </div>
      </LiveViewShortcutsProvider>
    </div>
  );
};

// TODO(nick,PC-917): withLiveViewContext needs a new version too that can do this. Complexity: VizierContextRouter
//  isn't just a context, it does manipulate behavior. Should it be in there? It's midway inside the stack; hard to move
const ContextualizedLiveView: React.FC = () => (
  <LayoutContextProvider>
    <DataDrawerContextProvider>
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
    </DataDrawerContextProvider>
  </LayoutContextProvider>
);

export default ContextualizedLiveView;
