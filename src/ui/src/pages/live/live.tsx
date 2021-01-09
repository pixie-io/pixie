import { scrollbarStyles, EditIcon } from 'pixie-components';
import VizierGRPCClientContext, { ClusterStatus, CLUSTER_STATUS_DISCONNECTED } from 'common/vizier-grpc-client-context';
import { ClusterContext } from 'common/cluster-context';
import MoveIcon from '@material-ui/icons/OpenWith';
import { ClusterInstructions } from 'containers/App/deploy-instructions';
import * as React from 'react';

import Link from '@material-ui/core/Link';
import IconButton from '@material-ui/core/IconButton';
import { Alert, AlertTitle } from '@material-ui/lab';
import {
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';

import Canvas from 'containers/live/canvas';
import CommandInput from 'containers/command-input/command-input';
import { withLiveViewContext } from 'containers/live/context';
import { LayoutContext } from 'context/layout-context';
import { ScriptContext } from 'context/script-context';
import { LiveTourContext } from 'containers/App/live-tour';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'containers/editor/editor';
import { ScriptLoader } from 'containers/live/script-loader';
import LiveViewShortcutsProvider from 'containers/live/shortcuts';
import LiveViewTitle from 'containers/live/title';
import LiveViewBreadcrumbs from 'containers/live/breadcrumbs';
import NavBars from 'containers/App/nav-bars';

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
  clusterUID: string;
}

const ClusterLoadingComponent = (props: ClusterLoadingProps) => {
  // Options:
  // 1. Name of the cluster
  const formatStatus = React.useMemo(
    () => props.clusterStatus.replace('CS_', '').toLowerCase(),
    [props.clusterStatus]);

  const actionMsg = React.useMemo(
    () => {
      if (props.clusterStatus === CLUSTER_STATUS_DISCONNECTED) {
        return (<div>Please redeploy Pixie to the cluster or choose another cluster.</div>);
      }
      return (
        <div>
          <div>
            Need help?&nbsp;
            <Link id='intercom-trigger'>Chat with us</Link>
            .
          </div>
        </div>
      );
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [props.clusterStatus, props.clusterUID]);

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

// Timeout before we display the cluster as unhealthy, in milliseconds.
const UNHEALTHY_CLUSTER_TIMEOUT = 5000;

const LiveView = () => {
  const classes = useStyles();

  const {
    saveEditorAndExecute, cancelExecution,
  } = React.useContext(ScriptContext);
  const { loading, clusterStatus } = React.useContext(VizierGRPCClientContext);
  const {
    setDataDrawerOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);

  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const { tourOpen } = React.useContext(LiveTourContext);
  const commandReallyOpen = commandOpen && !tourOpen;
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);
  const { selectedClusterPrettyName, selectedCluster } = React.useContext(ClusterContext);
  const [unhealthyClusterName, setUnhealthyCluster] = React.useState<string | null>(null);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute: () => saveEditorAndExecute(),
  };

  React.useEffect(() => {
    const listener = () => {
      cancelExecution?.();
    };

    window.addEventListener('beforeunload', listener);

    return () => {
      window.removeEventListener('beforeunload', listener);
    };
  }, [cancelExecution]);

  React.useEffect(() => {
    if (isMobile) {
      setWidgetsMoveable(false);
    }
  }, [isMobile]);

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
  // eslint-disable-next-line consistent-return
  React.useEffect(() => {
    if (loading) {
      setUnhealthyCluster(null);
      const intervalId = setInterval(() => {
        setUnhealthyCluster(selectedClusterPrettyName);
      }, UNHEALTHY_CLUSTER_TIMEOUT);
      return () => clearInterval(intervalId);
    }
  }, [loading, selectedClusterPrettyName]);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
        <NavBars />
        <div className={classes.content}>
          <LiveViewBreadcrumbs />
          <EditorSplitPanel className={classes.editorPanel}>
            <>
              <LiveViewTitle className={classes.title} />
              <ScriptOptions
                classes={classes}
                widgetsMoveable={widgetsMoveable}
                setWidgetsMoveable={setWidgetsMoveable}
              />
              {
                loading ? (
                  <div className='center-content'>
                    <ClusterLoadingComponent
                      clusterUnhealthy={
                        unhealthyClusterName
                        && unhealthyClusterName === selectedClusterPrettyName
                      }
                      clusterStatus={clusterStatus}
                      clusterName={selectedClusterPrettyName}
                      clusterUID={selectedCluster}
                    />
                  </div>
                ) : (
                  <>
                    <ScriptLoader />
                    <DataDrawerSplitPanel className={classes.mainPanel}>
                      <div className={classes.canvas} ref={canvasRef}>
                        <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                      </div>
                    </DataDrawerSplitPanel>
                    <CommandInput open={commandReallyOpen} onClose={toggleCommandOpen} />
                  </>
                )
              }
            </>
          </EditorSplitPanel>
        </div>
      </LiveViewShortcutsProvider>
    </div>
  );
};

export default withLiveViewContext(LiveView);
