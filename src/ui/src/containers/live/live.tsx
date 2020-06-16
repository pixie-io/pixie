import { scrollbarStyles } from 'common/mui-theme';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import EditIcon from 'components/icons/edit';
import PixieCommandIcon from 'components/icons/pixie-command';
import PixieLogo from 'components/icons/pixie-logo';
import { ClusterInstructions } from 'containers/vizier/deploy-instructions';
import * as React from 'react';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import ChevronRight from '@material-ui/icons/ChevronRight';
import ToggleButton from '@material-ui/lab/ToggleButton';

import Canvas from './canvas';
import ClusterSelector from './cluster-selector';
import CommandInput from './command-input';
import { withLiveViewContext } from './context';
import { ExecuteContext } from './context/execute-context';
import { LayoutContext } from './context/layout-context';
import { ScriptContext } from './context/script-context';
import { DataDrawerSplitPanel } from './data-drawer/data-drawer';
import { EditorSplitPanel } from './editor';
import ExecuteScriptButton from './execute-button';
import ProfileMenu from './profile-menu';
import { ScriptLoader } from './script-loader';
import LiveViewShortcuts from './shortcuts';
import LiveViewTitle from './title';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
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
    editorToggle: {
      border: 'none',
      borderRadius: '50%',
      color: theme.palette.action.active,
    },
    editorPanel: {
      display: 'flex',
      flexDirection: 'row',
    },
    canvas: {
      overflow: 'auto',
      marginLeft: theme.spacing(0.5),
    },
    pixieLogo: {
      opacity: 0.5,
      position: 'fixed',
      bottom: theme.spacing(1),
      right: theme.spacing(2),
      width: '48px',
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
  });
});

export const EditorOpener = () => {
  const { setEditorPanelOpen, editorPanelOpen } = React.useContext(LayoutContext);
  const openEditor = () => setEditorPanelOpen(true);
  const classes = useStyles();

  if (editorPanelOpen) {
    return null;
  }

  return (
    <Tooltip title='Open Editor'>
      <div className={classes.opener} onClick={openEditor}>
        <ChevronRight />
      </div>
    </Tooltip>
  );
};

const LiveView = () => {
  const classes = useStyles();

  const { execute } = React.useContext(ExecuteContext);
  const { loading } = React.useContext(VizierGRPCClientContext);
  const { setDataDrawerOpen, setEditorPanelOpen, editorPanelOpen, isMobile } = React.useContext(LayoutContext);
  const [canvasEditable, setCanvasEditable] = React.useState(editorPanelOpen);
  const toggleEdit = React.useCallback(() => {
    const editing = editorPanelOpen || canvasEditable;
    setEditorPanelOpen(!editing);
    setCanvasEditable(!editing);
  }, [editorPanelOpen, setEditorPanelOpen, canvasEditable, setCanvasEditable]);

  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = React.useCallback(() => setDrawerOpen((opened) => !opened), []);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': toggleEdit,
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute,
  };

  const { script, id } = React.useContext(ScriptContext);
  React.useEffect(() => {
    if (!script && !id) {
      setCommandOpen(true);
    }
  }, []);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <div className={classes.topBar}>
        <LiveViewTitle className={classes.title} />
        <ClusterSelector className={classes.clusterSelector} />
        <Tooltip title='Pixie Command'>
          <IconButton onClick={toggleCommandOpen}>
            <PixieCommandIcon color='primary' />
          </IconButton>
        </Tooltip>
        <ExecuteScriptButton />
        <Tooltip title='Edit View'>
          <ToggleButton
            disabled={isMobile}
            className={classes.editorToggle}
            selected={editorPanelOpen || canvasEditable}
            onChange={toggleEdit}
            value='editorOpened'
          >
            <EditIcon />
          </ToggleButton>
        </Tooltip>
        <ProfileMenu />
      </div>
      {
        loading ? <div className='center-content'><ClusterInstructions message='Connecting to cluster...' /></div> :
          <>
            <ScriptLoader />
            <DataDrawerSplitPanel className={classes.mainPanel}>
              <EditorSplitPanel className={classes.editorPanel}>
                <div className={classes.canvas} ref={canvasRef}>
                  <Canvas editable={canvasEditable} parentRef={canvasRef} />
                </div>
              </EditorSplitPanel>
            </DataDrawerSplitPanel>
            <Drawer open={drawerOpen} onClose={toggleDrawer}>
              <div>drawer content</div>
            </Drawer>
            <CommandInput open={commandOpen} onClose={toggleCommandOpen} />
            <PixieLogo className={classes.pixieLogo} />
            {canvasEditable ? <EditorOpener /> : null}
          </>
      }
    </div>
  );
};

export default withLiveViewContext(LiveView);
