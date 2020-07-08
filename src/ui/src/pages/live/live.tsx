import { scrollbarStyles } from 'common/mui-theme';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import MoveIcon from '@material-ui/icons/OpenWith';
import PixieCommandIcon from 'components/icons/pixie-command';
import { ClusterInstructions } from 'containers/App/deploy-instructions';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import ChevronRight from '@material-ui/icons/ChevronRight';
import ToggleButton from '@material-ui/lab/ToggleButton';

import clsx from 'clsx';
import Canvas from 'containers/live/canvas';
import ClusterSelector from 'containers/live/cluster-selector';
import CommandInput from 'containers/command-input/command-input';
import NewCommandInput from 'containers/new-command-input/new-command-input';
import { withLiveViewContext } from 'containers/live/context';
import { LayoutContext } from 'context/layout-context';
import { ScriptContext } from 'context/script-context';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'containers/editor/editor';
import ExecuteScriptButton from 'containers/live/execute-button';
import { ScriptLoader } from 'containers/live/script-loader';
import LiveViewShortcuts from 'containers/live/shortcuts';
import LiveViewTitle from 'containers/live/title';
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
    marginLeft: theme.spacing(8),
    display: 'flex',
    flex: 1,
    minWidth: 0,
    minHeight: 0,
  },
  title: {
    marginLeft: theme.spacing(2),
    flexGrow: 1,
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
  clusterSelector: {
    marginRight: theme.spacing(2),
  },
  opener: {
    position: 'absolute',
    top: theme.spacing(10) + 2, // Topbar height + border
    height: theme.spacing(6),
    width: theme.spacing(3),
    display: 'flex',
    alignItems: 'center',
    background: theme.palette.background.three,
    cursor: 'pointer',
    borderTopLeftRadius: 0,
    borderBottomLeftRadius: 0,
  },
  hidden: {
    display: 'none',
  },
}));

export const EditorOpener = () => {
  const { editorPanelOpen, isMobile, setEditorPanelOpen } = React.useContext(LayoutContext);
  const openEditor = () => setEditorPanelOpen(true);
  const classes = useStyles();

  if (isMobile) {
    return null;
  }

  return (
    <Tooltip title='Open Editor' className={clsx(editorPanelOpen && classes.hidden)}>
      <IconButton disabled={isMobile || editorPanelOpen} className={classes.opener} onClick={openEditor}>
        <ChevronRight />
      </IconButton>
    </Tooltip>
  );
};

const LiveView = () => {
  const classes = useStyles();

  const { pxl, id, saveEditorAndExecute } = React.useContext(ScriptContext);
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
    if (!pxl && !id) {
      setCommandOpen(true);
    }
  }, [id, pxl]);

  React.useEffect(() => {
    if (isMobile) {
      setWidgetsMoveable(false);
    }
  }, [isMobile]);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <NavBars>
        <LiveViewTitle className={classes.title} />
        <ClusterSelector className={classes.clusterSelector} />
        <Tooltip title='Pixie Command'>
          <IconButton disabled={commandOpen} onClick={toggleCommandOpen}>
            <PixieCommandIcon color='primary' />
          </IconButton>
        </Tooltip>
        <ExecuteScriptButton />
        {
          !isMobile
          && (
          <Tooltip title='Edit View'>
            <ToggleButton
              className={classes.moveWidgetToggle}
              selected={widgetsMoveable}
              onChange={() => setWidgetsMoveable(!widgetsMoveable)}
              value='moveWidget'
            >
              <MoveIcon />
            </ToggleButton>
          </Tooltip>
          )
        }
      </NavBars>
      {
        loading ? <div className='center-content'><ClusterInstructions message='Connecting to cluster...' /></div>
          : (
            <div className={classes.content}>
              <ScriptLoader />
              <DataDrawerSplitPanel className={classes.mainPanel}>
                <EditorSplitPanel className={classes.editorPanel}>
                  <div className={classes.canvas} ref={canvasRef}>
                    <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                  </div>
                </EditorSplitPanel>
              </DataDrawerSplitPanel>
              { localStorage.getItem('px-new-autocomplete') === 'true'
                ? <NewCommandInput open={commandOpen} onClose={toggleCommandOpen} />
                : <CommandInput open={commandOpen} onClose={toggleCommandOpen} /> }
              <EditorOpener />
            </div>
          )
      }
    </div>
  );
};

export default withLiveViewContext(LiveView);
