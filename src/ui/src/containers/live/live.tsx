import { scrollbarStyles } from 'common/mui-theme';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import MoveIcon from '@material-ui/icons/OpenWith';
import PixieCommandIcon from 'components/icons/pixie-command';
import { ClusterInstructions } from 'containers/App/deploy-instructions';
import * as React from 'react';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import ChevronRight from '@material-ui/icons/ChevronRight';
import ToggleButton from '@material-ui/lab/ToggleButton';

import clsx from 'clsx';
import Canvas from './canvas';
import ClusterSelector from './cluster-selector';
import CommandInput from './command-input';
import NewCommandInput from './new-command-input';
import { withLiveViewContext } from './context';
import { ExecuteContext } from '../../context/execute-context';
import { LayoutContext } from '../../context/layout-context';
import { ScriptContext } from '../../context/script-context';
import { DataDrawerSplitPanel } from '../data-drawer/data-drawer';
import { EditorSplitPanel } from './editor';
import ExecuteScriptButton from './execute-button';
import ProfileMenu from './profile-menu';
import { ScriptLoader } from './script-loader';
import LiveViewShortcuts from './shortcuts';
import LiveViewTitle from './title';

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
  topBar: {
    display: 'flex',
    padding: theme.spacing(1),
    alignItems: 'center',
    borderBottom: `solid 2px ${theme.palette.background.three}`,
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
  },
  canvas: {
    overflowY: 'auto',
    overflowX: 'hidden',
    marginLeft: theme.spacing(0.5),
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

  const { execute } = React.useContext(ExecuteContext);
  const { loading } = React.useContext(VizierGRPCClientContext);
  const {
    setDataDrawerOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);

  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = React.useCallback(() => setDrawerOpen((opened) => !opened), []);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute,
  };

  const { pxl, id } = React.useContext(ScriptContext);
  React.useEffect(() => {
    if (!pxl && !id) {
      setCommandOpen(true);
    }
  }, []);

  React.useEffect(() => {
    if (isMobile) {
      setWidgetsMoveable(false);
    }
  }, [isMobile]);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <div className={classes.topBar}>
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
        <ProfileMenu />
      </div>
      {
        loading ? <div className='center-content'><ClusterInstructions message='Connecting to cluster...' /></div>
          : (
            <>
              <ScriptLoader />
              <DataDrawerSplitPanel className={classes.mainPanel}>
                <EditorSplitPanel className={classes.editorPanel}>
                  <div className={classes.canvas} ref={canvasRef}>
                    <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                  </div>
                </EditorSplitPanel>
              </DataDrawerSplitPanel>
              <Drawer open={drawerOpen} onClose={toggleDrawer}>
                <div>drawer content</div>
              </Drawer>
              { localStorage.getItem('px-new-autocomplete') === 'true'
                ? <NewCommandInput open={commandOpen} onClose={toggleCommandOpen} />
                : <CommandInput open={commandOpen} onClose={toggleCommandOpen} /> }
              <EditorOpener />
            </>
          )
      }
    </div>
  );
};

export default withLiveViewContext(LiveView);
