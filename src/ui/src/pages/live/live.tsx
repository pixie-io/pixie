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

import { EditIcon, Footer, scrollbarStyles } from 'app/components';
import { GQLClusterStatus } from 'app/types/schema';
import { buildClass } from 'app/utils/build-class';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import {
  Alert, AlertTitle, IconButton, Link, Tooltip,
} from '@material-ui/core';
import MoveIcon from '@material-ui/icons/OpenWith';

import { Copyright } from 'configurable/copyright';
import { ClusterContext } from 'app/common/cluster-context';
import { DataDrawerContextProvider } from 'app/context/data-drawer-context';
import EditorContextProvider, { EditorContext } from 'app/context/editor-context';
import { LayoutContext, LayoutContextProvider } from 'app/context/layout-context';
import { ScriptContext, ScriptContextProvider } from 'app/context/script-context';
import { ResultsContextProvider } from 'app/context/results-context';
import { Script } from 'app/utils/script-bundle';

import { ClusterInstructions } from 'app/containers/App/deploy-instructions';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import NavBars from 'app/containers/App/nav-bars';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { DataDrawerSplitPanel } from 'app/containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'app/containers/editor/editor';
import Canvas from 'app/containers/live/canvas';
import LiveViewBreadcrumbs from 'app/containers/live/breadcrumbs';
import { ScriptLoader } from 'app/containers/live/script-loader';
import LiveViewShortcutsProvider from 'app/containers/live/shortcuts';
import { CONTACT_ENABLED } from 'app/containers/constants';
import ExecuteScriptButton from 'app/containers/live/execute-button';
import ClusterSelector from 'app/containers/live/cluster-selector';
import { LiveTourContextProvider } from 'app/containers/App/live-tour';
import { PixieAPIClient, PixieAPIContext } from 'app/api';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  main: {
    flexGrow: 1,
    display: 'flex',
    flexFlow: 'column nowrap',
    overflow: 'auto',
  },
  mainContent: {
    marginLeft: theme.spacing(8),
    paddingTop: theme.spacing(2),
    display: 'flex',
    flex: '1 0 auto',
    minWidth: 0,
    minHeight: 0,
    flexDirection: 'column',
    [theme.breakpoints.down('sm')]: {
      // Sidebar is disabled.
      marginLeft: 0,
    },
    overflowY: 'auto',
    overflowX: 'hidden',
  },
  embeddedMain: {
    marginLeft: 0,
  },
  widgetMain: {
    paddingTop: 0,
  },
  mainFooter: {
    marginLeft: theme.spacing(8),
    flex: '0 0 auto',
  },
  spacer: {
    flex: 1,
  },
  execute: {
    display: 'flex',
  },
  combinedBreadcrumbsAndRun: {
    display: 'flex',
    marginRight: theme.spacing(3),
  },
  nestedBreadcrumbs: {
    flex: 1,
  },
  nestedRun: {
    display: 'flex',
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
  dataDrawer: {
    width: `calc(100% - ${theme.spacing(8)})`,
    position: 'absolute',
    pointerEvents: 'none',
    marginLeft: theme.spacing(8),
    height: '100%',
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
    marginLeft: theme.spacing(0.5),
    height: '100%',
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
    width: theme.spacing(4),
    height: theme.spacing(4),
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

  if (isMobile) {
    return <></>;
  }

  return (
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
  );
};

interface ClusterLoadingProps {
  clusterPrettyName: string;
  clusterStatus: GQLClusterStatus;
  script: Script;
  healthy: boolean;
}

const ClusterLoadingComponent = ({
  clusterPrettyName, clusterStatus, script, healthy,
}: ClusterLoadingProps) => {
  const { loading: loadingAvailableScripts } = React.useContext(ScriptsContext);

  const formattedStatus = React.useMemo(
    () => clusterStatus.replace('CS_', '').toLowerCase(),
    [clusterStatus]);

  if (clusterStatus === GQLClusterStatus.CS_DISCONNECTED) {
    return (
      <div>
        <Alert severity='error'>
          <AlertTitle>
            {`Cluster '${clusterPrettyName}' is disconnected`}
          </AlertTitle>
          <div>
            {`Pixie instrumentation on '${clusterPrettyName}' is ${formattedStatus}.`}
          </div>
          <div>
            Please redeploy Pixie to the cluster or choose another cluster.
          </div>
        </Alert>
      </div>
    );
  }

  if (clusterStatus !== GQLClusterStatus.CS_HEALTHY) {
    return (
      <div>
        <Alert severity='error'>
          <AlertTitle>
            {`Cluster '${clusterPrettyName}' unavailable`}
          </AlertTitle>
          <div>
            {`Pixie instrumentation on '${clusterPrettyName}' is ${formattedStatus}.`}
          </div>
          {CONTACT_ENABLED && (
          <div>
            <div>
              Need help?&nbsp;
              <Link id='intercom-trigger'>Chat with us</Link>
              .
            </div>
          </div>
          )}
        </Alert>
      </div>
    );
  }

  if (!loadingAvailableScripts && !script) {
    return <div> Script name invalid, choose a new script in the dropdown</div>;
  }

  if (!healthy) {
    return <ClusterInstructions message='Connecting to cluster...' />;
  }

  return <></>;
};

const Nav: React.FC<{
  widgetsMoveable: boolean,
  setWidgetsMoveable: React.Dispatch<React.SetStateAction<boolean>>,
}> = ({ widgetsMoveable, setWidgetsMoveable }) => {
  const classes = useStyles();
  const {
    embedState: { isEmbedded },
  } = React.useContext(LiveRouteContext);

  if (isEmbedded) {
    return <></>;
  }

  return <>
    <NavBars>
      <ClusterSelector />
      <div className={classes.spacer} />
      <ScriptOptions
        classes={classes}
        widgetsMoveable={widgetsMoveable}
        setWidgetsMoveable={setWidgetsMoveable}
      />
      <div className={classes.execute}>
        <ExecuteScriptButton />
      </div>
    </NavBars>
    <div className={classes.dataDrawer}>
      <DataDrawerSplitPanel />
    </div>
  </>;
};

const BreadcrumbsWithOptionalRun: React.FC = () => {
  const classes = useStyles();
  const {
    embedState: { isEmbedded, widget },
  } = React.useContext(LiveRouteContext);

  if (widget) {
    return <></>;
  }

  if (!isEmbedded) {
    return <LiveViewBreadcrumbs />;
  }

  return <div className={classes.combinedBreadcrumbsAndRun}>
    <div className={classes.nestedBreadcrumbs}>
      <LiveViewBreadcrumbs />
    </div>
    <div className={classes.nestedRun}>
      <ExecuteScriptButton />
    </div>
  </div>;
};

const LiveView: React.FC = () => {
  const classes = useStyles();

  const { selectedClusterName, selectedClusterPrettyName, selectedClusterStatus } = React.useContext(ClusterContext);
  const { script, args, cancelExecution } = React.useContext(ScriptContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { isMobile, setEditorPanelOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);
  const {
    embedState: { isEmbedded, widget },
  } = React.useContext(LiveRouteContext);

  const hotkeyHandlers = {
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    execute: () => saveEditor(),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    // TODO(philkuz,PC-917) Pixie Command shortcut has been removed while we work to resolve its quirks.
    'pixie-command': () => {},
  };

  const canvasRef = React.useRef<HTMLDivElement>(null);

  const cloudClient = (React.useContext(PixieAPIContext) as PixieAPIClient).getCloudClient();

  const healthy = cloudClient && selectedClusterStatus === GQLClusterStatus.CS_HEALTHY;

  // Healthy might flicker on and off. We only care to show the loading state for first load,
  // and want to ignore future health check failures. So we use healthyOnce to start as false
  // and transition to true once. After the transition, it will always stay true.
  const [healthyOnce, setHealthyOnce] = React.useState(false);
  React.useCallback(() => {
    setHealthyOnce((prev) => (prev || healthy));
  }, [healthy]);

  // Opens the editor if the current script is a scratch script.
  React.useEffect(() => {
    if (script?.id === SCRATCH_SCRIPT.id && script?.code === SCRATCH_SCRIPT.code) {
      setEditorPanelOpen(true);
    }
  }, [script?.code, script?.id, setEditorPanelOpen]);

  // Cancel execution if the window unloads.
  React.useEffect(() => {
    const listener = () => {
      cancelExecution?.();
    };

    window.addEventListener('beforeunload', listener);

    return () => {
      window.removeEventListener('beforeunload', listener);
    };
  }, [cancelExecution]);

  // Hides the movable widgets button on mobile.
  React.useEffect(() => {
    if (isMobile) {
      setWidgetsMoveable(false);
    }
  }, [isMobile]);

  // Enable escape key to stop setting widgets as movable.
  React.useEffect(() => {
    const handleEsc = (event) => {
      if (event.keyCode === 27) {
        setWidgetsMoveable(false);
      }
    };
    window.addEventListener('keydown', handleEsc);

    return () => {
      window.removeEventListener('keydown', handleEsc);
    };
  }, [setWidgetsMoveable]);

  if (!selectedClusterName || !args) return null;

  return (
    <div className={classes.root}>
      <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
        <Nav
          widgetsMoveable={widgetsMoveable}
          setWidgetsMoveable={setWidgetsMoveable}
        />
        <EditorSplitPanel>
          <div className={classes.main}>
            <div className={buildClass(
              classes.mainContent,
              isEmbedded && classes.embeddedMain,
              widget && classes.widgetMain,
            )}>
              <BreadcrumbsWithOptionalRun />
              {(selectedClusterStatus === GQLClusterStatus.CS_HEALTHY && script && healthy) ? (
                <div className={classes.canvas} ref={canvasRef}>
                  <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                </div>
              ) : (
                <div className='center-content'>
                  <ClusterLoadingComponent
                    clusterPrettyName={selectedClusterPrettyName}
                    clusterStatus={selectedClusterStatus}
                    script={script}
                    healthy={healthyOnce}
                  />
                </div>
              )}
            </div>
            {!isEmbedded && <div className={classes.mainFooter}>
              <Footer copyright={Copyright} />
            </div>}
          </div>
        </EditorSplitPanel>
      </LiveViewShortcutsProvider>
    </div>
  );
};

const ContextualizedLiveView: React.FC = () => (
  <LayoutContextProvider>
    <LiveTourContextProvider>
      <DataDrawerContextProvider>
        <ResultsContextProvider>
          <ScriptContextProvider>
            <EditorContextProvider>
              <ScriptLoader />
              <LiveView />
            </EditorContextProvider>
          </ScriptContextProvider>
        </ResultsContextProvider>
      </DataDrawerContextProvider>
    </LiveTourContextProvider>
  </LayoutContextProvider>
);

export default ContextualizedLiveView;
