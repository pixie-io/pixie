import { LIVE_VIEW_EDITOR_OPENED_KEY, useLocalStorage } from 'common/localstorage';
import { scrollbarStyles } from 'common/mui-theme';
import EditIcon from 'components/icons/edit';
import MagicIcon from 'components/icons/magic';
import PixieLogo from 'components/icons/pixie-logo';
import LazyPanel from 'components/lazy-panel';
import * as React from 'react';
import { GlobalHotKeys } from 'react-hotkeys';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import MenuIcon from '@material-ui/icons/Menu';
import MoveIcon from '@material-ui/icons/OpenWith';
import ShareIcon from '@material-ui/icons/Share';
import ToggleButton from '@material-ui/lab/ToggleButton';

import Canvas from './canvas';
import CommandInput from './command-input';
import { LiveContext, withLiveContextProvider } from './context';
import DataDrawer from './data-drawer';
import Editor from './editor';
import ExecuteScriptButton from './execute-button';
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
      display: 'flex',
      flexDirection: 'column',
      borderTopStyle: 'solid',
      borderTopColor: theme.palette.background.three,
      borderTopWidth: theme.spacing(0.25),
    },
    mainRow: {
      display: 'flex',
      flexDirection: 'row',
      flex: 3,
      minHeight: 0,
    },
    dataDrawer: {
      flex: 2,
      minHeight: 0,
    },
    drawerToggle: {
      height: theme.spacing(4),
      display: 'flex',
      cursor: 'pointer',
    },
    editorToggle: {
      border: 'none',
      borderRadius: '50%',
      color: theme.palette.action.active,
    },
    editor: {
      flex: 2,
      minWidth: 0,
      borderRightStyle: 'solid',
      borderRightColor: theme.palette.background.three,
      borderRightWidth: theme.spacing(0.25),
    },
    canvas: {
      flex: 3,
      minWidth: 0,
      overflow: 'auto',
    },
    pixieLogo: {
      opacity: 0.5,
      position: 'fixed',
      bottom: theme.spacing(1),
      right: theme.spacing(2),
      width: '48px',
    },
  });
});

const LiveView = () => {
  const classes = useStyles();

  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = React.useCallback(() => setDrawerOpen((opened) => !opened), []);

  const [editorOpen, setEditorOpen] = useLocalStorage<boolean>(LIVE_VIEW_EDITOR_OPENED_KEY, false);
  const toggleEditor = React.useCallback(() => setEditorOpen((opened) => !opened), []);

  const [canvasEditable, setCanvasEditable] = React.useState<boolean>(false);
  const toggleCanvasEditable = React.useCallback(() => setCanvasEditable((editable) => !editable), []);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const { toggleDataDrawer, executeScript } = React.useContext(LiveContext);

  const hotkeyHandlers = React.useMemo(() => {
    return {
      'pixie-command': toggleCommandOpen,
      'toggle-editor': toggleEditor,
      'toggle-data-drawer': toggleDataDrawer,
      executeScript,
    };
  }, [executeScript]);

  useInitScriptLoader();

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <div className={classes.topBar}>
        {/* <IconButton disabled={true} onClick={toggleDrawer}>
          <MenuIcon />
        </IconButton> */}
        <LiveViewTitle className={classes.title} />
        <ExecuteScriptButton />
        {/* <IconButton disabled={true}>
          <ShareIcon />
        </IconButton> */}
        <ToggleButton
          className={classes.editorToggle}
          selected={editorOpen}
          onChange={toggleEditor}
          value='editorOpened'
        >
          <EditIcon />
        </ToggleButton>
        <ToggleButton
          className={classes.editorToggle}
          selected={canvasEditable}
          onChange={toggleCanvasEditable}
          value='canvasEditable'
        >
          <MoveIcon />
        </ToggleButton>
        <IconButton onClick={toggleCommandOpen}>
          <MagicIcon />
        </IconButton>
      </div>
      <div className={classes.main}>
        <div className={classes.mainRow}>
          <LazyPanel className={classes.editor} show={editorOpen}>
            <Editor onClose={toggleEditor} />
          </LazyPanel>
          <div className={classes.canvas}>
            <Canvas editable={canvasEditable} />
          </div>
        </div>
        <DataDrawer />
      </div>
      <Drawer open={drawerOpen} onClose={toggleDrawer}>
        <div>drawer content</div>
      </Drawer>
      <CommandInput open={commandOpen} onClose={toggleCommandOpen} />
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};

export default withLiveContextProvider(LiveView);
