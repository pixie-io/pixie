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

import { OpenWith as MoveIcon } from '@mui/icons-material';
import {
  Alert, AlertTitle, IconButton, Link, Tooltip,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useFlags } from 'launchdarkly-react-client-sdk';

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { isPixieEmbedded } from 'app/common/embed-context';
import { EditIcon, Footer, scrollbarStyles } from 'app/components';
import { CommandPalette } from 'app/components/command-palette';
import {
  CommandPaletteContext,
  CommandPaletteContextProvider,
} from 'app/components/command-palette/command-palette-context';
import { Spinner } from 'app/components/spinner/spinner';
import { ClusterInstructions } from 'app/containers/App/deploy-instructions';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { LiveTourContextProvider } from 'app/containers/App/live-tour';
import NavBars from 'app/containers/App/nav-bars';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { DataDrawerSplitPanel } from 'app/containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'app/containers/editor/editor';
import { LiveViewBreadcrumbs } from 'app/containers/live/breadcrumbs';
import Canvas from 'app/containers/live/canvas';
import ClusterSelector from 'app/containers/live/cluster-selector';
import ExecuteScriptButton from 'app/containers/live/execute-button';
import { ScriptLoader } from 'app/containers/live/script-loader';
import ShareButton from 'app/containers/live/share-button';
import LiveViewShortcutsProvider from 'app/containers/live/shortcuts';
import { SetStateFunc } from 'app/context/common';
import { DataDrawerContextProvider } from 'app/context/data-drawer-context';
import { EditorContext, EditorContextProvider } from 'app/context/editor-context';
import { LayoutContext, LayoutContextProvider } from 'app/context/layout-context';
import { ResultsContext, ResultsContextProvider } from 'app/context/results-context';
import { ScriptContext, ScriptContextProvider } from 'app/context/script-context';
import { GQLClusterStatus } from 'app/types/schema';
import { stableSerializeArgs } from 'app/utils/args-utils';
import { buildClass } from 'app/utils/build-class';
import { showIntercomTrigger, triggerID } from 'app/utils/intercom';
import { containsMutation } from 'app/utils/pxl';
import { Script } from 'app/utils/script-bundle';
import { Copyright } from 'configurable/copyright';

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
  middle: {
    flex: 1,
    paddingLeft: theme.spacing(2),
    paddingRight: theme.spacing(2),
    minWidth: 0,
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
  dataDrawer: {
    width: `calc(100% - ${theme.spacing(8)})`,
    position: 'absolute',
    pointerEvents: 'none',
    marginLeft: theme.spacing(8),
    height: '100%',
  },
  centerContent: {
    width: '100%',
    height: '100%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
  },
  canvas: {
    marginLeft: theme.spacing(0.5),
    height: '100%',
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
  statusMessage: {
    marginBottom: theme.spacing(1),
    marginTop: theme.spacing(1),
  },
  alert: {
    marginTop: theme.spacing(2),
    marginLeft: theme.spacing(3),
    marginRight: theme.spacing(3),
  },
}));

const ScriptOptions = React.memo<{ widgetsMoveable: boolean, setWidgetsMoveable: SetStateFunc<boolean> }>(({
  widgetsMoveable, setWidgetsMoveable,
}) => {
  const classes = useStyles();
  const {
    editorPanelOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);

  const toggleEditorOpen = React.useCallback(() => setEditorPanelOpen((open) => !open), [setEditorPanelOpen]);
  const toggleWidgetsMoveable = React.useCallback(
    () => setWidgetsMoveable((moveable) => !moveable), [setWidgetsMoveable]);

  if (isMobile) {
    return <></>;
  }

  return (
    <div className={classes.iconPanel}>
      <ShareButton classes={classes} />
      <Tooltip title={`${editorPanelOpen ? 'Close' : 'Open'} editor`} className={classes.iconButton}>
        <IconButton className={classes.iconButton} onClick={toggleEditorOpen}>
          <EditIcon className={editorPanelOpen ? classes.iconActive : classes.iconInactive} />
        </IconButton>
      </Tooltip>
      <Tooltip title={`${widgetsMoveable ? 'Disable' : 'Enable'} move widgets`} className={classes.iconButton}>
        <IconButton onClick={toggleWidgetsMoveable}>
          <MoveIcon className={widgetsMoveable ? classes.iconActive : classes.iconInactive} />
        </IconButton>
      </Tooltip>
    </div>
  );
});
ScriptOptions.displayName = 'ScriptOptions';

interface ClusterLoadingProps {
  clusterPrettyName: string;
  clusterStatus: GQLClusterStatus;
  clusterStatusMessage: string;
  script: Script;
  healthy: boolean;
}

const ClusterLoadingComponent = React.memo<ClusterLoadingProps>(({
  clusterPrettyName, clusterStatus, clusterStatusMessage, script, healthy,
}) => {
  const classes = useStyles();

  const { loading: loadingCluster } = React.useContext(ClusterContext);
  const { loading: loadingAvailableScripts } = React.useContext(ScriptsContext);
  const { loading: loadingResults, streaming: streamingResults } = React.useContext(ResultsContext);

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
            Please redeploy Pixie to the cluster or choose another cluster.
          </div>
        </Alert>
      </div>
    );
  }

  if (!loadingCluster && !([GQLClusterStatus.CS_HEALTHY, GQLClusterStatus.CS_DEGRADED].includes(clusterStatus))) {
    return (
      <div>
        <Alert severity='error'>
          <AlertTitle>
            {`Cluster '${clusterPrettyName}' is ${formattedStatus}`}
          </AlertTitle>
          {
            (clusterStatusMessage && clusterStatusMessage.length)
            && <div className={classes.statusMessage}>
              {clusterStatusMessage}
            </div>
          }
          {showIntercomTrigger() && (
          <div>
            <div>
              Need help?&nbsp;
              <Link id={triggerID}>Chat with us</Link>
              .
            </div>
          </div>
          )}
        </Alert>
      </div>
    );
  }

  if (!loadingAvailableScripts && !script) {
    return <div>Script name invalid, choose a new script in the dropdown</div>;
  }

  if (!loadingCluster && !healthy) {
    return <ClusterInstructions message='Connecting to cluster...' />;
  }

  if (loadingCluster || (loadingResults && !streamingResults)) {
    return <Spinner />;
  }

  // Cluster is healthy, script is set, scripts are loaded, results are loaded but this component was still invoked.
  // Tell the user they may run a script here.
  return <div>Select a script, then click &quot;Run&quot;.</div>;
});
ClusterLoadingComponent.displayName = 'ClusterLoadingComponent';

const Nav: React.FC<{
  widgetsMoveable: boolean,
  setWidgetsMoveable: React.Dispatch<React.SetStateAction<boolean>>,
}> = React.memo(({ widgetsMoveable, setWidgetsMoveable }) => {
  const classes = useStyles();
  const { showCommandPalette } = useFlags();

  if (isPixieEmbedded()) {
    return <></>;
  }

  return <>
    <NavBars>
      <ClusterSelector />
      <div className={classes.middle}>
        {showCommandPalette && <CommandPalette />}
      </div>
      <ScriptOptions
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
});
Nav.displayName = 'Nav';

const BreadcrumbsWithOptionalRun = React.memo(() => {
  const classes = useStyles();
  const { embedState: { widget } } = React.useContext(LiveRouteContext);

  if (widget) {
    return <></>;
  }

  if (!isPixieEmbedded()) {
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
});
BreadcrumbsWithOptionalRun.displayName = 'BreadcrumbsWithOptionalRun';

const LiveView = React.memo(() => {
  const classes = useStyles();

  const {
    selectedClusterName,
    selectedClusterPrettyName,
    selectedClusterStatus,
    selectedClusterStatusMessage,
    loading: loadingCluster,
  } = React.useContext(ClusterContext);
  const {
    script, args, cancelExecution, manual,
  } = React.useContext(ScriptContext);
  const {
    tables, error, mutationInfo, loading: loadingResults, streaming: streamingResults,
  } = React.useContext(ResultsContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { isMobile, setEditorPanelOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);
  const { embedState: { widget } } = React.useContext(LiveRouteContext);
  const { open: isCommandPaletteOpen, setOpen: setCommandPaletteOpen } = React.useContext(CommandPaletteContext);
  const isEmbedded = isPixieEmbedded();

  const hotkeyHandlers = React.useMemo(() => ({
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    'toggle-command-palette': () => setCommandPaletteOpen((paletteOpen) => !paletteOpen),
    // See shortcuts.tsx for why this one is conditionally defined.
    // Short version, only one of 'execute' | 'command-palette-cta' can have an action or else the wrong one "wins".
    ...(isCommandPaletteOpen ? {} : {
      execute: () => saveEditor(),
    }),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
  }), [setEditorPanelOpen, saveEditor, setDataDrawerOpen, isCommandPaletteOpen, setCommandPaletteOpen]);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  const cloudClient = (React.useContext(PixieAPIContext) as PixieAPIClient).getCloudClient();

  const healthy = cloudClient && (selectedClusterStatus === GQLClusterStatus.CS_HEALTHY
    || selectedClusterStatus === GQLClusterStatus.CS_DEGRADED);
  const degraded = selectedClusterStatus === GQLClusterStatus.CS_DEGRADED;

  // Healthy might flicker on and off. We only care to show the loading state for first load,
  // and want to ignore future health check failures. So we use healthyOnce to start as false
  // and transition to true once. After the transition, it will always stay true.
  const [healthyOnce, setHealthyOnce] = React.useState(false);
  React.useEffect(() => {
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

  const scroller = React.useRef<HTMLDivElement>(null);
  const serializedArgs = stableSerializeArgs(args);
  const scrollId = !(script?.id) ? null : `${script.id}-${serializedArgs}`;
  React.useEffect(() => {
    // Scroll up when, from the user's perspective, the script changes. Re-running it counts.
    if (scrollId) scroller.current?.scrollTo({ top: 0 });
  }, [scrollId]);

  if (!selectedClusterName || !args) return null;

  const willRun = script && (!containsMutation(script?.code) || manual);
  const showResults = script && healthyOnce && (
    (loadingCluster && willRun) || tables.size || loadingResults || streamingResults || error || mutationInfo);

  return (
    <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
      <div className={classes.root}>
        <Nav
          widgetsMoveable={widgetsMoveable}
          setWidgetsMoveable={setWidgetsMoveable}
        />
        <EditorSplitPanel>
          <div className={classes.main} ref={scroller}>
            <div className={buildClass(
              classes.mainContent,
              isEmbedded && classes.embeddedMain,
              widget && classes.widgetMain,
            )}>
              <BreadcrumbsWithOptionalRun />
              {degraded ? (
                <div className={classes.alert}>
                  <Alert severity='warning'>
                    <AlertTitle>Data may be incomplete</AlertTitle>
                      {`Cluster is in a degraded state: ${selectedClusterStatusMessage}`}
                  </Alert>
                </div>) : null
              }
              {showResults ? (
                <div className={classes.canvas} ref={canvasRef}>
                  <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                </div>
              ) : (
                <div className={classes.centerContent}>
                  <ClusterLoadingComponent
                    clusterPrettyName={selectedClusterPrettyName}
                    clusterStatus={selectedClusterStatus}
                    clusterStatusMessage={selectedClusterStatusMessage}
                    script={script}
                    healthy={healthyOnce}
                  />
                </div>
              )}
            </div>
            {!isEmbedded && <div className={classes.mainFooter}>
              {/* eslint-disable-next-line react-memo/require-usememo */}
              <Footer copyright={Copyright} />
            </div>}
          </div>
        </EditorSplitPanel>
      </div>
    </LiveViewShortcutsProvider>
  );
});
LiveView.displayName = 'LiveView';

// eslint-disable-next-line react-memo/require-memo
const ContextualizedLiveView: React.FC = () => (
  <LayoutContextProvider>
    <LiveTourContextProvider>
      <DataDrawerContextProvider>
        <ResultsContextProvider>
          <ScriptContextProvider>
            <CommandPaletteContextProvider>
              <EditorContextProvider>
                <ScriptLoader />
                <LiveView />
              </EditorContextProvider>
            </CommandPaletteContextProvider>
          </ScriptContextProvider>
        </ResultsContextProvider>
      </DataDrawerContextProvider>
    </LiveTourContextProvider>
  </LayoutContextProvider>
);
ContextualizedLiveView.displayName = 'ContextualizedLiveView';

export default ContextualizedLiveView;
