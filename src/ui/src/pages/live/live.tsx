import { scrollbarStyles } from 'common/mui-theme';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import MoveIcon from '@material-ui/icons/OpenWith';
import PixieCommandIcon from 'components/icons/pixie-command';
import { ClusterInstructions } from 'containers/App/deploy-instructions';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import EditIcon from 'components/icons/edit';

import Canvas from 'containers/live/canvas';
import CommandInput from 'containers/command-input/command-input';
import NewCommandInput from 'containers/new-command-input/new-command-input';
import { withLiveViewContext } from 'containers/live/context';
import { LayoutContext } from 'context/layout-context';
import { ScriptContext } from 'context/script-context';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'containers/editor/editor';
import { ScriptLoader } from 'containers/live/script-loader';
import LiveViewShortcutsProvider from 'containers/live/shortcuts';
import LiveViewTitle from 'containers/live/title';
import LiveViewBreadcrumbs from 'containers/live/breadcrumbs';
import NavBars from 'containers/App/nav-bars';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import { useFlags } from 'launchdarkly-react-client-sdk';

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

const LiveView = () => {
  const classes = useStyles();
  const { newAutoComplete } = useFlags();

  const {
    pxl, id, saveEditorAndExecute, cancelExecution,
  } = React.useContext(ScriptContext);
  const { loading } = React.useContext(VizierGRPCClientContext);
  const {
    setDataDrawerOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);

  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute: () => saveEditorAndExecute(),
  };

  React.useEffect(() => {
    const listener = () => {
      if (cancelExecution != null) {
        cancelExecution();
      }
    };

    window.addEventListener('beforeunload', listener);

    return () => {
      window.removeEventListener('beforeunload', listener);
    };
  }, [cancelExecution]);

  React.useEffect(() => {
    if (!pxl && !id) {
      setCommandOpen(true);
    }
  }, [id, pxl]);

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

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcutsProvider handlers={hotkeyHandlers}>
        <NavBars />
        <div className={classes.content}>
          <LiveViewBreadcrumbs toggleCommandOpen={toggleCommandOpen} commandOpen={commandOpen} />
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
                    <ClusterInstructions message='Connecting to cluster...' />
                  </div>
                ) : (
                  <>
                    <ScriptLoader />
                    <DataDrawerSplitPanel className={classes.mainPanel}>
                      <div className={classes.canvas} ref={canvasRef}>
                        <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                      </div>
                    </DataDrawerSplitPanel>
                    { newAutoComplete
                      ? <NewCommandInput open={commandOpen} onClose={toggleCommandOpen} />
                      : <CommandInput open={commandOpen} onClose={toggleCommandOpen} /> }
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
