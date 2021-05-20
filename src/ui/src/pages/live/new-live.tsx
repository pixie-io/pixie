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

import { scrollbarStyles, EditIcon } from '@pixie-labs/components';
import { GQLClusterStatus as ClusterStatus } from '@pixie-labs/api';
import {
  makeStyles, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import {
  Button, Link, Tooltip, IconButton,
} from '@material-ui/core';
import { Alert, AlertTitle } from '@material-ui/lab';
import MoveIcon from '@material-ui/icons/OpenWith';

import { ClusterContext } from 'common/cluster-context';
import { DataDrawerContextProvider } from 'context/data-drawer-context';
import EditorContextProvider, { EditorContext } from 'context/editor-context';
import { LayoutContext, LayoutContextProvider } from 'context/layout-context';
import { ScriptContext, ScriptContextProvider } from 'context/new-script-context';
import { ResultsContext, ResultsContextProvider } from 'context/results-context';

import { ClusterInstructions } from 'containers/App/deploy-instructions';
import NavBars from 'containers/App/nav-bars';
import { VizierContextRouter } from 'containers/App/vizier-routing';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'containers/editor/new-editor';
import Canvas from 'containers/live/new-canvas';
import LiveViewBreadcrumbs from 'containers/live/new-breadcrumbs';
import { ScriptLoader } from 'containers/live/new-script-loader';
import LiveViewShortcutsProvider from 'containers/live/shortcuts';
import { CONTACT_ENABLED } from 'containers/constants';
import { useListClusters } from '@pixie-labs/api-react';

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
interface ClusterLoadingProps {
  clusterUnhealthy: boolean;
  clusterStatus: ClusterStatus;
  clusterName: string | null;
}

const ClusterLoadingComponent = (props: ClusterLoadingProps) => {
  // Options:
  // 1. Name of the cluster
  const formatStatus = React.useMemo(
    () => props.clusterStatus.replace('CS_', '').toLowerCase(),
    [props.clusterStatus]);

  const actionMsg = React.useMemo(
    (): JSX.Element => {
      if (props.clusterStatus === ClusterStatus.CS_DISCONNECTED) {
        return (<div>Please redeploy Pixie to the cluster or choose another cluster.</div>);
      }

      if (CONTACT_ENABLED) {
        return (
          <div>
            <div>
              Need help?&nbsp;
              <Link id='intercom-trigger'>Chat with us</Link>
              .
            </div>
          </div>
        );
      }
      return <div />;
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [props.clusterStatus, props.clusterName]);

  return (
    <>
      {props.clusterUnhealthy ? (
        <div>
          <Alert severity='error'>
            <AlertTitle>
              {`Cluster '${props.clusterName}' unavailable`}
            </AlertTitle>
            <div>
              {`Pixie instrumentation on '${props.clusterName}' is ${formatStatus}.`}
            </div>
            {actionMsg}
          </Alert>
        </div>
      ) : (
        <ClusterInstructions message='Connecting to cluster...' />
      )}
    </>
  );
};

const LiveView: React.FC = () => {
  const classes = useStyles();

  const { selectedClusterName, selectedClusterPrettyName } = React.useContext(ClusterContext);
  const { script, args } = React.useContext(ScriptContext);
  const results = React.useContext(ResultsContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { setEditorPanelOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

  // These two variables track whether a script has been executed on a cluster.
  // When the first script starts executing, hasStartedLoading shall be set true.
  // When that script stops, hasFinishedLoading shall be set true.
  // When the cluster changes, both are set false.
  const [hasStartedLoadingCluster, setHasStartedLoadingCluster] = React.useState<boolean>(false);
  const [hasFinishedLoadingCluster, setHasFinishedLoadingCluster] = React.useState<boolean>(false);

  const [clusters, clustersLoading, error] = useListClusters();

  const clusterStatus: ClusterStatus = React.useMemo(() => {
    if (error || clustersLoading || !clusters) {
      return ClusterStatus.CS_UNKNOWN;
    }

    const cluster = clusters.find((c) => c.clusterName === selectedClusterName);
    if (!cluster) {
      return ClusterStatus.CS_UNKNOWN;
    }
    return cluster.status;
  }, [clusters, clustersLoading, error, selectedClusterName]);

  const hotkeyHandlers = {
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    execute: () => saveEditor(),
    // TODO(philkuz,PC-917) enable the other commands when their components are added in.
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    'pixie-command': () => {},
  };

  const canvasRef = React.useRef<HTMLDivElement>(null);

  // The following three useEffects determine how much a user has
  // interacted with a cluster. We need to show a connecting modal
  // when the user has not successfully made a connection. When they
  // do successfully connect with the cluster, subsequent scripts
  // executed should not cause the connecting modal to show up.
  //
  // The effects should execute actions sequentially whenever a user
  // selects a new Cluster for their live session.

  // 1. we set both loading variables as false. No scripts have started/finished
  // 2. When the script loading beings, we flag the cluster as starting.
  // 3. Once the script has finished loading, we flag the first load as finished.
  //
  React.useEffect(() => {
    // When we reset the cluster name, we reset this to false as results.loading will always trail.
    setHasStartedLoadingCluster(false);
    setHasFinishedLoadingCluster(false);
  }, [selectedClusterPrettyName, setHasStartedLoadingCluster]);

  React.useEffect(() => {
    if (results.loading) {
      setHasStartedLoadingCluster(true);
    }
  }, [results.loading, setHasStartedLoadingCluster]);

  React.useEffect(() => {
    if (!results.loading && hasStartedLoadingCluster) {
      setHasFinishedLoadingCluster(true);
    }
  }, [results.loading, hasStartedLoadingCluster, setHasFinishedLoadingCluster]);

  const clusterUnhealthy = !clustersLoading && clusterStatus !== ClusterStatus.CS_HEALTHY;

  if (!selectedClusterName || !script || !args) return null;

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
              !hasFinishedLoadingCluster || clusterUnhealthy ? (
                <div className='center-content'>
                  <ClusterLoadingComponent
                    clusterUnhealthy={clusterUnhealthy}
                    clusterStatus={clusterStatus}
                    clusterName={selectedClusterPrettyName}
                  />
                </div>
              ) : (

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
