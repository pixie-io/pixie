import { scrollbarStyles } from 'common/mui-theme';
import EditIcon from 'components/icons/edit';
import PixieCommandIcon from 'components/icons/pixie-command';
import PixieLogo from 'components/icons/pixie-logo';
import * as React from 'react';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import MoveIcon from '@material-ui/icons/OpenWith';
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
import { useInitScriptLoader } from './script-loader';
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
      margin: theme.spacing(1),
      alignItems: 'center',
    },
    title: {
      marginLeft: theme.spacing(2),
      flexGrow: 1,
    },
    main: {
      flex: 1,
      minHeight: 0,
      borderTopStyle: 'solid',
      borderTopColor: theme.palette.background.three,
      borderTopWidth: theme.spacing(0.25),
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
  });
});

const LiveView = () => {
  const classes = useStyles();

  const { execute } = React.useContext(ExecuteContext);
  const { setDataDrawerOpen, setEditorPanelOpen, editorPanelOpen, isMobile } = React.useContext(LayoutContext);
  const toggleEditor = React.useCallback(() => setEditorPanelOpen((open) => !open), [setEditorPanelOpen]);

  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = React.useCallback(() => setDrawerOpen((opened) => !opened), []);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': toggleEditor,
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute,
  };

  useInitScriptLoader();

  const { script, id } = React.useContext(ScriptContext);
  React.useEffect(() => {
    if (!script && !id) {
      setCommandOpen(true);
    }
  }, []);

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <div className={classes.topBar}>
        {/* <IconButton disabled={true} onClick={toggleDrawer}>
          <MenuIcon />
        </IconButton> */}
        <LiveViewTitle className={classes.title} />
        <ClusterSelector className={classes.clusterSelector} />
        <Tooltip title='Pixie Command'>
          <IconButton onClick={toggleCommandOpen}>
            <PixieCommandIcon color='primary' />
          </IconButton>
        </Tooltip>
        <ExecuteScriptButton />
        {/* <IconButton disabled={true}>
          <ShareIcon />
        </IconButton> */}
        <Tooltip title={editorPanelOpen ? 'Close editor' : 'Open editor'}>
          <ToggleButton
            disabled={isMobile}
            className={classes.editorToggle}
            selected={editorPanelOpen}
            onChange={toggleEditor}
            value='editorOpened'
          >
            <EditIcon />
          </ToggleButton>
        </Tooltip>
        <ProfileMenu />
      </div>
      <DataDrawerSplitPanel className={classes.main}>
        <EditorSplitPanel className={classes.editorPanel}>
          <div className={classes.canvas}>
            <Canvas editable={editorPanelOpen} />
          </div>
        </EditorSplitPanel>
      </DataDrawerSplitPanel>
      <Drawer open={drawerOpen} onClose={toggleDrawer}>
        <div>drawer content</div>
      </Drawer>
      <CommandInput open={commandOpen} onClose={toggleCommandOpen} />
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};

export default withLiveViewContext(LiveView);
